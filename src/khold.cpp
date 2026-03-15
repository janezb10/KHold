/*
 * KHold - Press-and-Hold Character Selection
 * Copyright (C) 2026 Janez B.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "khold.h"
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx-utils/utf8.h>
#include <sstream>
#include <nlohmann/json.hpp>

namespace fcitx {

void KHoldCandidateWord::select(InputContext *inputContext) const {
    inputContext->commitString(text().stringAt(0));
    auto *state = inputContext->propertyFor(&khold_->factory());
    state->reset();
}

KHoldState::KHoldState(KHold *khold, InputContext *ic) : khold_(khold), ic_(ic) {}

KHoldState::~KHoldState() {
    if (timer_) timer_->setEnabled(false);
}

void KHoldState::reset() {
    if (timer_) timer_->setEnabled(false);
    holding_ = false;
    lookupTableActive_ = false;
    currentKeyUTF8_.clear();
    currentKeySym_ = FcitxKey_None;
    currentCandidates_.clear();
    ic_->inputPanel().reset();
    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    ic_->updatePreedit();
}

void KHoldState::flush() {
    if (holding_ && !lookupTableActive_ && !currentKeyUTF8_.empty()) {
        ic_->commitString(currentKeyUTF8_);
    }
    reset();
}

bool KHoldState::handleKeyEvent(const KeyEvent &event) {
    KeySym sym = event.key().sym();

    if (event.isRelease()) {
        if (holding_ && sym == currentKeySym_) {
            if (lookupTableActive_) {
                holding_ = false;
            } else {
                if (timer_) timer_->setEnabled(false);
                ic_->commitString(currentKeyUTF8_);
                reset();
            }
            return true;
        }
        return lookupTableActive_;
    }

    if (holding_ && sym == currentKeySym_) {
        return true; 
    }

    if (lookupTableActive_) {
        int idx = event.key().digitSelection();
        if (idx >= 0 && idx < static_cast<int>(currentCandidates_.size())) {
            ic_->inputPanel().candidateList()->candidate(idx).select(ic_);
            return true;
        }
        if (sym == FcitxKey_Escape || sym == FcitxKey_BackSpace) {
            reset();
            return true;
        }
        flush();
        return false;
    }

    auto states = event.key().states();
    if (states.test(KeyState::Ctrl) || states.test(KeyState::Alt) || states.test(KeyState::Super)) {
        return false;
    }

    if (const auto* entry = khold_->getEntry(sym)) {
        holding_ = true;
        currentKeySym_ = sym;
        currentKeyUTF8_ = entry->keyUTF8;
        currentCandidates_ = entry->candidates;

        uint64_t targetTime = now(CLOCK_MONOTONIC) + khold_->delay() * 1000;
        if (!timer_) {
            timer_ = khold_->instance()->eventLoop().addTimeEvent(
                CLOCK_MONOTONIC, targetTime, 0, [this](EventSourceTime *, uint64_t) {
                    this->onTimer();
                    return false;
                });
        } else {
            timer_->setTime(targetTime);
        }
        timer_->setEnabled(true);
        timer_->setOneShot();
        return true;
    }

    if (holding_) {
        flush();
        return false;
    }

    return false;
}

void KHoldState::onTimer() {
    if (!holding_) return;
    lookupTableActive_ = true;
    
    auto candidateList = std::make_unique<CommonCandidateList>();
    KeyList selectionKeys;
    for (int i = 0; i < static_cast<int>(currentCandidates_.size()); ++i) {
        candidateList->append(std::make_unique<KHoldCandidateWord>(khold_, currentCandidates_[i]));
        selectionKeys.emplace_back(static_cast<KeySym>(FcitxKey_1 + i));
    }
    
    candidateList->setLayoutHint(CandidateLayoutHint::Horizontal);
    candidateList->setSelectionKey(selectionKeys);
    ic_->inputPanel().setCandidateList(std::move(candidateList));
    
    Text preedit;
    preedit.append(currentKeyUTF8_, TextFormatFlag::HighLight);
    ic_->inputPanel().setPreedit(preedit); 
    
    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    ic_->updatePreedit();
}

KHold::KHold(Instance *instance)
    : instance_(instance),
      factory_([this](InputContext &ic) { return new KHoldState(this, &ic); }) {
    instance_->inputContextManager().registerProperty("khold_state", &factory_);
    handlerKey_ = instance_->watchEvent(EventType::InputContextKeyEvent, 
                                     EventWatcherPhase::PreInputMethod,
                                     [this](Event &event) { onKeyEvent(event); });
    handlerReset_ = instance_->watchEvent(EventType::InputContextReset, 
                                     EventWatcherPhase::PreInputMethod,
                                     [this](Event &event) { onResetEvent(event); });
    handlerFocusOut_ = instance_->watchEvent(EventType::InputContextFocusOut, 
                                     EventWatcherPhase::PreInputMethod,
                                     [this](Event &event) { onResetEvent(event); });
    KHold::reloadConfig();
    FCITX_INFO() << "KHold: Initialized";
}

KHold::~KHold() = default;

void KHold::reloadConfig() {
    readAsIni(config_, "conf/khold.conf");
    entryMap_.clear();
    for (const auto& entry : config_.entries.value()) {
        Key k(entry.key.value());
        if (k.isValid()) {
            KHoldEntryInternal internal;
            internal.keyUTF8 = Key::keySymToUTF8(k.sym());
            internal.candidates = entry.candidates.value();
            entryMap_[k.sym()] = std::move(internal);
        }
    }
}

void KHold::setConfig(const RawConfig &rawConfig) {
    config_.load(rawConfig, true);
    
    std::string bulkJson = config_.bulkImportJson.value();
    if (!bulkJson.empty()) {
        try {
            nlohmann::json j = nlohmann::json::parse(bulkJson);
            if (j.contains("keys") && j["keys"].is_object()) {
                auto newEntries = config_.entries.value();
                for (auto const& [key_str, cand_val] : j["keys"].items()) {
                    std::vector<std::string> cands;
                    if (cand_val.is_array()) {
                        for (const auto& item : cand_val) {
                            if (item.is_string()) cands.push_back(item.get<std::string>());
                        }
                    } else if (cand_val.is_string()) {
                        std::string cands_str = cand_val.get<std::string>();
                        for (uint32_t cp : utf8::MakeUTF8CharRange(cands_str)) {
                            cands.push_back(utf8::UCS4ToUTF8(cp));
                        }
                    }
                    if (!key_str.empty() && !cands.empty()) {
                        bool found = false;
                        for (auto& entry : newEntries) {
                            if (entry.key.value() == key_str) {
                                entry.candidates.setValue(cands);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            KHoldEntry newEntry;
                            newEntry.key.setValue(key_str);
                            newEntry.candidates.setValue(cands);
                            newEntries.push_back(std::move(newEntry));
                        }
                    }
                }
                config_.entries.setValue(std::move(newEntries));
            }
        } catch (...) {}
        config_.bulkImportJson.setValue("");
    }

    std::string bulk = config_.bulkImport.value();
    if (!bulk.empty()) {
        auto entries = config_.entries.value();
        std::stringstream ss(bulk);
        std::string line;
        while (std::getline(ss, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string keyStr = line.substr(0, pos);
                std::string candsStr = line.substr(pos + 1);
                std::vector<std::string> cands;
                for (uint32_t cp : utf8::MakeUTF8CharRange(candsStr)) {
                    cands.push_back(utf8::UCS4ToUTF8(cp));
                }
                if (!keyStr.empty() && !cands.empty()) {
                    bool found = false;
                    for (auto& entry : entries) {
                        if (entry.key.value() == keyStr) {
                            entry.candidates.setValue(cands);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        KHoldEntry newEntry;
                        newEntry.key.setValue(keyStr);
                        newEntry.candidates.setValue(cands);
                        entries.push_back(std::move(newEntry));
                    }
                }
            }
        }
        config_.entries.setValue(std::move(entries));
        config_.bulkImport.setValue("");
    }

    safeSaveAsIni(config_, "conf/khold.conf");
    reloadConfig();
}

const KHoldEntryInternal* KHold::getEntry(KeySym sym) const {
    auto it = entryMap_.find(sym);
    return (it != entryMap_.end()) ? &it->second : nullptr;
}

void KHold::onKeyEvent(Event &event) const {
    auto &keyEvent = static_cast<KeyEvent &>(event);
    auto *state = keyEvent.inputContext()->propertyFor(&factory_);
    if (state->handleKeyEvent(keyEvent)) {
        keyEvent.filterAndAccept();
    }
}

void KHold::onResetEvent(Event &event) const {
    auto &icEvent = static_cast<InputContextEvent &>(event);
    auto *state = icEvent.inputContext()->propertyFor(&factory_);
    state->flush();
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::KHoldFactory);

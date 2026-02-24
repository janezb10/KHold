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

namespace fcitx {

void KHoldCandidateWord::select(InputContext *inputContext) const {
    inputContext->commitString(text().stringAt(0));
    auto *state = inputContext->propertyFor(&khold_->factory());
    state->reset();
}

KHoldState::KHoldState(KHold *khold, InputContext *ic) : khold_(khold), ic_(ic) {}

KHoldState::~KHoldState() {
    if (timer_) {
        timer_->setEnabled(false);
    }
}

void KHoldState::reset() {
    if (timer_) {
        timer_->setEnabled(false);
    }
    holding_ = false;
    lookupTableActive_ = false;
    currentKeyStr_.clear();
    currentKeySym_ = FcitxKey_None;
    currentCandidates_.clear();
    ic_->inputPanel().reset();
    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    ic_->updatePreedit();
}

bool KHoldState::handleKeyEvent(const KeyEvent &event) {
    KeySym sym = event.key().sym();

    if (event.isRelease()) {
        if (holding_ && sym == currentKeySym_) {
            if (timer_ && timer_->isEnabled()) {
                // KRATEK PRITISK: Hitra sprostitev
                timer_->setEnabled(false);
                holding_ = false;
                ic_->commitString(currentKeyStr_);
                reset();
                return true;
            }
            holding_ = false;
        }
        return lookupTableActive_;
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
        reset();
        return false;
    }

    if (!event.key().hasModifier()) {
        if (const auto* candidates = khold_->getCandidates(sym)) {
            if (holding_) return true; // Ignoriraj sistemski repeat
            
            holding_ = true;
            currentKeySym_ = sym;
            currentKeyStr_ = event.key().toString(); // toString kličemo le ob prvem pritisku
            currentCandidates_ = *candidates;

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
    }

    if (holding_) {
        timer_->setEnabled(false);
        holding_ = false;
        ic_->commitString(currentKeyStr_);
        reset();
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
    preedit.append(currentKeyStr_, TextFormatFlag::HighLight);
    ic_->inputPanel().setPreedit(preedit); 
    
    ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
    ic_->updatePreedit();
}

KHold::KHold(Instance *instance)
    : instance_(instance),
      factory_([this](InputContext &ic) { return new KHoldState(this, &ic); }) {
    instance_->inputContextManager().registerProperty("khold_state", &factory_);
    handler_ = instance_->watchEvent(EventType::InputContextKeyEvent, 
                                     EventWatcherPhase::PreInputMethod,
                                     [this](Event &event) { onKeyEvent(event); });
    KHold::reloadConfig();
    FCITX_INFO() << "KHold: Initialized (Optimized)";
}

KHold::~KHold() = default;

void KHold::reloadConfig() {
    readAsIni(config_, "conf/khold.conf");
    entryMap_.clear();
    for (const auto& entry : config_.entries.value()) {
        Key k(entry.key.value());
        if (k.isValid()) {
            entryMap_[k.sym()] = entry.candidates.value();
        }
    }
}

void KHold::setConfig(const RawConfig &config) {
    config_.load(config, true);
    safeSaveAsIni(config_, "conf/khold.conf");
    reloadConfig();
}

const std::vector<std::string>* KHold::getCandidates(KeySym sym) const {
    auto it = entryMap_.find(sym);
    if (it != entryMap_.end()) {
        return &it->second;
    }
    return nullptr;
}

void KHold::onKeyEvent(Event &event) const
{
    auto &keyEvent = static_cast<KeyEvent &>(event);
    auto *state = keyEvent.inputContext()->propertyFor(&factory_);
    if (state->handleKeyEvent(keyEvent)) {
        keyEvent.filterAndAccept();
    }
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::KHoldFactory);

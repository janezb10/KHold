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

#ifndef FCITX_KHOLD_H_
#define FCITX_KHOLD_H_

#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/instance.h>
#include <fcitx-config/configuration.h>
#include <fcitx-config/option.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/handlertable.h>
#include <fcitx-utils/i18n.h>

#include <utility>
#include <unordered_map>

namespace fcitx {

struct KHoldEntryInternal {
    std::string keyUTF8;
    std::vector<std::string> candidates;
};

FCITX_CONFIGURATION(KHoldEntry,
    Option<std::string> key{this, "Key", _("Trigger Key")};
    Option<std::vector<std::string>> candidates{this, "Candidates", _("Candidates")};
);

FCITX_CONFIGURATION(KHoldConfig,
    Option<int, IntConstrain> delay{this, "Delay", _("Long Press Delay (ms)"), 600, IntConstrain(100, 2000)};
    Option<std::string> bulkImport{this, "Bulk Import (key=cands)", _("Bulk Import (key=cands)"), ""};
    Option<std::string> bulkImportJson{this, "Bulk Import (JSON)", _("Bulk Import (JSON string)"), ""};
    OptionWithAnnotation<std::vector<KHoldEntry>, ListDisplayOptionAnnotation>
        entries{this, "Entries", _("Long Press Entries"), {}, {}, {}, 
        ListDisplayOptionAnnotation("Key")};
);

class KHold;

class KHoldCandidateWord : public CandidateWord {
public:
    KHoldCandidateWord(KHold *khold, std::string text)
        : CandidateWord(Text(std::move(text))), khold_(khold) {}
    void select(InputContext *inputContext) const override;

private:
    KHold *khold_;
};

class KHoldState : public InputContextProperty {
public:
    KHoldState(KHold *khold, InputContext *ic);
    ~KHoldState() override;

    void reset();
    void flush();
    bool handleKeyEvent(const KeyEvent &event);
    void onTimer();

    KHold *khold_;
    InputContext *ic_;
    std::unique_ptr<EventSourceTime> timer_;
    bool holding_ = false;
    bool committed_ = false;
    bool lookupTableActive_ = false;
    std::string currentKeyUTF8_;
    KeySym currentKeySym_ = FcitxKey_None;
    std::vector<std::string> currentCandidates_;
};

class KHold : public AddonInstance {
public:
    KHold(Instance *instance);
    ~KHold() override;

    Instance *instance() const { return instance_; }
    auto &factory() { return factory_; }

    void reloadConfig() override;
    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override;

    const KHoldEntryInternal* getEntry(KeySym sym) const;
    int delay() const { return config_.delay.value(); }

private:
    void onKeyEvent(Event &event) const;
    void onResetEvent(Event &event) const;
    Instance *instance_;
    KHoldConfig config_;
    std::unordered_map<KeySym, KHoldEntryInternal> entryMap_;
    FactoryFor<KHoldState> factory_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> handlerKey_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> handlerReset_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> handlerFocusOut_;
};

class KHoldFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new KHold(manager->instance());
    }
};

} // namespace fcitx

#endif // FCITX_KHOLD_H_

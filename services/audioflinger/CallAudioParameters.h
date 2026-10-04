// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project
#pragma once
#include <array>
#include <media/AudioParameter.h>
#include <utils/Errors.h>

namespace android {
// Keep the same AudioParameter parser used by the production forwarding path.
inline bool containsCallAudioParameter(const String8& pairs) {
    static constexpr std::array keys = {
        "vsid", "call_state", "call_type", "crs_call", "all_call_states",
    };
    const AudioParameter parameters(pairs);
    String8 value;
    for (const auto* key : keys) {
        if (parameters.get(String8(key), value) == NO_ERROR) return true;
    }
    return false;
}
} // namespace android

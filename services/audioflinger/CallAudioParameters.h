// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project
#pragma once
#include <array>
#include <media/AudioParameter.h>
#include <utils/Errors.h>

namespace android {
// HAL keys that change a live voice call, grouped by the callers that own them.
// Vendor voice-session control, set by the call-audio bridge.
inline constexpr unsigned kCallAudioSessionKeys = 1u << 0;
// TTY, hearing-aid and voice-quality settings, set by platform telephony services.
inline constexpr unsigned kCallAudioFeatureKeys = 1u << 1;
// Bluetooth voice links, set by AudioService and the Bluetooth stack.
inline constexpr unsigned kCallAudioBluetoothKeys = 1u << 2;

// Returns the groups of all protected keys in pairs, or 0. Matching is exact.
// Keep the same AudioParameter parser used by the production forwarding path.
inline unsigned callAudioParameterGroups(const String8& pairs) {
    struct Key {
        const char* name;
        unsigned group;
    };
    static constexpr std::array keys = {
        Key{"vsid", kCallAudioSessionKeys},
        Key{"call_state", kCallAudioSessionKeys},
        Key{"call_type", kCallAudioSessionKeys},
        Key{"crs_call", kCallAudioSessionKeys},
        Key{"all_call_states", kCallAudioSessionKeys},
        Key{"tty_mode", kCallAudioFeatureKeys},
        Key{"HACSetting", kCallAudioFeatureKeys},
        Key{"hd_voice", kCallAudioFeatureKeys},
        Key{"st_enable", kCallAudioFeatureKeys}, // slow talk
        Key{"volume_boost", kCallAudioFeatureKeys},
        Key{"device_mute", kCallAudioFeatureKeys},
        Key{"CRS_volume", kCallAudioFeatureKeys},
        Key{"BT_SCO", kCallAudioBluetoothKeys},
        Key{"bt_headset_name", kCallAudioBluetoothKeys},
        Key{"bt_headset_nrec", kCallAudioBluetoothKeys},
        Key{"bt_wbs", kCallAudioBluetoothKeys},
        Key{"bt_swb", kCallAudioBluetoothKeys},
        Key{"bt_lc3_swb", kCallAudioBluetoothKeys},
        Key{"bt_ble", kCallAudioBluetoothKeys},
        Key{"hfp_enable", kCallAudioBluetoothKeys},
        Key{"hfp_set_sampling_rate", kCallAudioBluetoothKeys},
        Key{"hfp_volume", kCallAudioBluetoothKeys},
        Key{"A2dpSuspended", kCallAudioBluetoothKeys},
        Key{"LeAudioSuspended", kCallAudioBluetoothKeys},
    };
    const AudioParameter parameters(pairs);
    String8 value;
    unsigned groups = 0;
    for (const auto& key : keys) {
        if (parameters.get(String8(key.name), value) == NO_ERROR) groups |= key.group;
    }
    return groups;
}

inline bool containsCallAudioParameter(const String8& pairs) {
    return callAudioParameterGroups(pairs) != 0;
}
} // namespace android

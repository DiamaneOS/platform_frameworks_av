// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "callaudioparameters_tests"

#include "../CallAudioParameters.h"

#include <gtest/gtest.h>

using namespace android;

namespace {

unsigned groups(const char* pairs) {
    return callAudioParameterGroups(String8(pairs));
}

TEST(CallAudioParametersTests, SessionKeys) {
    for (const char* pairs : {"vsid=297816064", "call_state=2", "call_type=UNKNOWN",
            "crs_call=false", "all_call_states", "vsid", "vsid=0;call_state=1"}) {
        EXPECT_EQ(kCallAudioSessionKeys, groups(pairs)) << pairs;
    }
}

TEST(CallAudioParametersTests, CallFeatureKeys) {
    for (const char* pairs : {"tty_mode=tty_full", "tty_mode", "HACSetting=ON",
            "hd_voice=false", "st_enable=true", "volume_boost=on",
            "device_mute=true;direction=tx", "CRS_volume=5"}) {
        EXPECT_EQ(kCallAudioFeatureKeys, groups(pairs)) << pairs;
    }
}

TEST(CallAudioParametersTests, BluetoothVoiceKeys) {
    for (const char* pairs : {"BT_SCO=on", "bt_headset_name=Car;bt_headset_nrec=on;bt_wbs=on",
            "bt_swb=65535", "bt_lc3_swb=off", "bt_ble=on", "hfp_enable=true",
            "hfp_set_sampling_rate=16000", "hfp_volume=7", "A2dpSuspended=true",
            "LeAudioSuspended=false"}) {
        EXPECT_EQ(kCallAudioBluetoothKeys, groups(pairs)) << pairs;
    }
}

TEST(CallAudioParametersTests, MixedKeysReportEveryGroup) {
    EXPECT_EQ(kCallAudioSessionKeys | kCallAudioFeatureKeys | kCallAudioBluetoothKeys,
              groups("screen_state=on;vsid=0;tty_mode=tty_off;BT_SCO=off"));
    EXPECT_EQ(kCallAudioFeatureKeys | kCallAudioBluetoothKeys,
              groups("HACSetting=OFF;bt_wbs=off"));
}

TEST(CallAudioParametersTests, OtherKeysAreNotProtected) {
    for (const char* pairs : {"", ";", "screen_state=on", "rotation=90", "direction=rx",
            "isCRSsupported", "reconfigA2dp=true", "TTY_MODE=tty_full", "hacsetting=ON",
            " tty_mode=tty_full", "bt_wbs_x=on", "xvsid=1", "=vsid"}) {
        EXPECT_EQ(0u, groups(pairs)) << pairs;
        EXPECT_FALSE(containsCallAudioParameter(String8(pairs))) << pairs;
    }
}

} // namespace

// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "DeviceCameraListsTest"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "../utils/DeviceCameraLists.h"

using namespace android::camera3;

namespace {

const std::string kCertificate(64, 'a');

} // namespace

TEST(DeviceCameraListsTest, AppIdentities) {
    std::vector<AppIdentity> apps = parseAppIdentities("com.example.app:" + kCertificate);
    ASSERT_EQ(1u, apps.size());
    EXPECT_EQ("com.example.app", apps[0].package);
    EXPECT_EQ(std::vector<uint8_t>(32, 0xaa), apps[0].certificateSha256);

    apps = parseAppIdentities("com.example.one:" + std::string(64, '0') + " , com.example.two:" +
            std::string(64, 'f'));
    ASSERT_EQ(2u, apps.size());
    EXPECT_EQ("com.example.two", apps[1].package);
    EXPECT_EQ(std::vector<uint8_t>(32, 0xff), apps[1].certificateSha256);
}

TEST(DeviceCameraListsTest, MalformedAppEntriesAreSkippedNotMatched) {
    EXPECT_TRUE(parseAppIdentities("").empty());
    for (const std::string& entry : {
            std::string("com.example.app"),                     // no certificate
            "com.example.app:" + kCertificate.substr(1),        // 63 digits
            "com.example.app:" + kCertificate + "a",            // 65 digits
            "com.example.app:" + std::string(64, 'A'),          // uppercase
            "com.example.app:" + std::string(63, 'a') + "g",    // not hex
            "app:" + kCertificate,                              // one part only
            "com.1example.app:" + kCertificate,                 // part starts with a digit
            "com..app:" + kCertificate,                         // empty part
            ":" + kCertificate}) {
        EXPECT_TRUE(parseAppIdentities(entry).empty()) << entry;
    }
    std::vector<AppIdentity> apps = parseAppIdentities("bad:" + kCertificate + ",com.example.ok:" +
            kCertificate);
    ASSERT_EQ(1u, apps.size());
    EXPECT_EQ("com.example.ok", apps[0].package);
}

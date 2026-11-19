// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "DeviceCameraLists"

#include "utils/DeviceCameraLists.h"

#include <android-base/properties.h>
#include <android-base/strings.h>
#include <log/log.h>

namespace android {

namespace camera3 {

namespace {

bool isNameStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isNameChar(char c) {
    return isNameStart(c) || (c >= '0' && c <= '9') || c == '_';
}

// Android's rule for a package name: two or more parts that start with a letter, joined by dots.
bool isPackageName(const std::string& name) {
    std::vector<std::string> parts = base::Split(name, ".");
    if (parts.size() < 2) return false;
    for (const std::string& part : parts) {
        if (part.empty() || !isNameStart(part[0])) return false;
        for (char c : part) {
            if (!isNameChar(c)) return false;
        }
    }
    return true;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

} // namespace

std::vector<std::string> parseVendorTagNames(const std::string& value) {
    std::vector<std::string> names;
    for (const std::string& entry : base::Split(value, ",")) {
        std::string name = base::Trim(entry);
        // A section and a tag name, made of name characters and dots only.
        bool valid = name.find('.') != std::string::npos && name.front() != '.' && name.back() != '.';
        for (char c : name) {
            if (!isNameChar(c) && c != '.') valid = false;
        }
        if (valid) {
            names.push_back(name);
        } else if (!name.empty()) {
            ALOGW("%s: skipped a malformed vendor tag name", kBlockedVendorTagsProperty);
        }
    }
    return names;
}

std::vector<AppIdentity> parseAppIdentities(const std::string& value) {
    std::vector<AppIdentity> apps;
    for (const std::string& entry : base::Split(value, ",")) {
        std::string trimmed = base::Trim(entry);
        if (trimmed.empty()) continue;
        size_t colon = trimmed.find(':');
        AppIdentity app;
        bool valid = colon != std::string::npos && trimmed.size() - colon - 1 == 64;
        if (valid) {
            app.package = trimmed.substr(0, colon);
            valid = isPackageName(app.package);
        }
        for (size_t i = colon + 1; valid && i < trimmed.size(); i += 2) {
            int high = hexValue(trimmed[i]), low = hexValue(trimmed[i + 1]);
            valid = high >= 0 && low >= 0;
            app.certificateSha256.push_back(static_cast<uint8_t>(high << 4 | low));
        }
        if (valid) {
            apps.push_back(std::move(app));
        } else {
            ALOGW("%s: skipped a malformed entry", kPixelModeExemptAppsProperty);
        }
    }
    return apps;
}

std::vector<std::string> blockedVendorTagNames() {
    return parseVendorTagNames(base::GetProperty(kBlockedVendorTagsProperty, ""));
}

std::vector<AppIdentity> pixelModeExemptApps() {
    return parseAppIdentities(base::GetProperty(kPixelModeExemptAppsProperty, ""));
}

} // namespace camera3

} // namespace android

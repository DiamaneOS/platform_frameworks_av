// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#ifndef ANDROID_SERVERS_CAMERA_DEVICECAMERALISTS_H
#define ANDROID_SERVERS_CAMERA_DEVICECAMERALISTS_H

#include <cstdint>
#include <string>
#include <vector>

namespace android {

namespace camera3 {

// Lists a device gives the camera service in read-only vendor properties, so what is known about one
// device's camera stays in that device's build. Without the property a list is empty.

// Vendor tags clients may not set, by full name (section.name), comma-separated.
inline constexpr char kBlockedVendorTagsProperty[] = "ro.camera.blocked_vendor_tags";

// Apps whose capture requests are not checked for a sensor pixel mode that matches their streams,
// comma-separated, each as package:certificate, the SHA-256 of the app's signing certificate in 64
// lowercase hex digits.
inline constexpr char kPixelModeExemptAppsProperty[] = "ro.camera.pixel_mode_exempt_apps";

struct AppIdentity {
    std::string package;
    std::vector<uint8_t> certificateSha256;
};

// Parsing, separate from the property reads for tests. Malformed entries are skipped.
std::vector<std::string> parseVendorTagNames(const std::string& value);
std::vector<AppIdentity> parseAppIdentities(const std::string& value);

std::vector<std::string> blockedVendorTagNames();
std::vector<AppIdentity> pixelModeExemptApps();

} // namespace camera3

} // namespace android

#endif

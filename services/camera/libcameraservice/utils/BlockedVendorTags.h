// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#ifndef ANDROID_SERVERS_CAMERA_BLOCKEDVENDORTAGS_H
#define ANDROID_SERVERS_CAMERA_BLOCKEDVENDORTAGS_H

#include <string>
#include <vector>

#include <camera/CameraMetadata.h>
#include <camera/VendorTagDescriptor.h>

namespace android {

namespace camera3 {

// Vendor tags clients may not set: the names a device lists for a camera provider that misbehaves
// with them (ro.camera.blocked_vendor_tags, DeviceCameraLists.h). Names the vendor tags do not
// define are skipped.
std::vector<uint32_t> getBlockedVendorTags(const VendorTagDescriptor* vTags,
        const std::vector<std::string>& names);

// The same, resolved through the vendor tags of the camera's provider.
std::vector<uint32_t> getBlockedVendorTags(metadata_vendor_id_t vendorTagId,
        const std::vector<std::string>& names);

// Removes the tags from the metadata. Returns whether any was set.
bool eraseTags(CameraMetadata& metadata, const std::vector<uint32_t>& tags);

} // namespace camera3

} // namespace android

#endif

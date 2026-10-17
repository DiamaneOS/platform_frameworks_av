// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#ifndef ANDROID_SERVERS_CAMERA_BLOCKEDVENDORTAGS_H
#define ANDROID_SERVERS_CAMERA_BLOCKEDVENDORTAGS_H

#include <vector>

#include <camera/CameraMetadata.h>
#include <camera/VendorTagDescriptor.h>

namespace android {

namespace camera3 {

// Vendor tags clients may not set. Qualcomm's in-sensor zoom makes the FP6 camera write outside
// its buffers. Names the vendor tags do not define are skipped.
std::vector<uint32_t> getBlockedVendorTags(const VendorTagDescriptor* vTags);

// The same, resolved through the vendor tags of the camera's provider.
std::vector<uint32_t> getBlockedVendorTags(metadata_vendor_id_t vendorTagId);

// Removes the tags from the metadata. Returns whether any was set.
bool eraseTags(CameraMetadata& metadata, const std::vector<uint32_t>& tags);

} // namespace camera3

} // namespace android

#endif

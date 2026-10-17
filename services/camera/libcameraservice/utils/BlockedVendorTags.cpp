// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "BlockedVendorTags"

#include "utils/BlockedVendorTags.h"

namespace android {

namespace camera3 {

namespace {

constexpr const char* kBlockedTagNames[] = {
    "org.codeaurora.qcamera3.sessionParameters.EnableInsensorZoom",
    "org.codeaurora.qcamera3.sessionParameters.EnableSnapshotOnlyInsensorZoom",
};

} // namespace

std::vector<uint32_t> getBlockedVendorTags(const VendorTagDescriptor* vTags) {
    std::vector<uint32_t> tags;
    for (const char* name : kBlockedTagNames) {
        uint32_t tag;
        if (CameraMetadata::getTagFromName(name, vTags, &tag) == OK) {
            tags.push_back(tag);
        }
    }
    return tags;
}

std::vector<uint32_t> getBlockedVendorTags(metadata_vendor_id_t vendorTagId) {
    sp<VendorTagDescriptor> vTags = VendorTagDescriptor::getGlobalVendorTagDescriptor();
    if (vTags == nullptr || vTags->getTagCount() <= 0) {
        sp<VendorTagDescriptorCache> cache = VendorTagDescriptorCache::getGlobalVendorTagCache();
        if (cache != nullptr) {
            cache->getVendorTagDescriptor(vendorTagId, &vTags);
        }
    }
    return getBlockedVendorTags(vTags.get());
}

bool eraseTags(CameraMetadata& metadata, const std::vector<uint32_t>& tags) {
    bool erased = false;
    for (uint32_t tag : tags) {
        if (metadata.exists(tag)) {
            metadata.erase(tag);
            erased = true;
        }
    }
    return erased;
}

} // namespace camera3

} // namespace android

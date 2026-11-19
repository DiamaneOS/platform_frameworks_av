// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "BlockedVendorTags"

#include "utils/BlockedVendorTags.h"

namespace android {

namespace camera3 {

std::vector<uint32_t> getBlockedVendorTags(const VendorTagDescriptor* vTags,
        const std::vector<std::string>& names) {
    std::vector<uint32_t> tags;
    for (const std::string& name : names) {
        uint32_t tag;
        if (CameraMetadata::getTagFromName(name.c_str(), vTags, &tag) == OK) {
            tags.push_back(tag);
        }
    }
    return tags;
}

std::vector<uint32_t> getBlockedVendorTags(metadata_vendor_id_t vendorTagId,
        const std::vector<std::string>& names) {
    sp<VendorTagDescriptor> vTags = VendorTagDescriptor::getGlobalVendorTagDescriptor();
    if (vTags == nullptr || vTags->getTagCount() <= 0) {
        sp<VendorTagDescriptorCache> cache = VendorTagDescriptorCache::getGlobalVendorTagCache();
        if (cache != nullptr) {
            cache->getVendorTagDescriptor(vendorTagId, &vTags);
        }
    }
    return getBlockedVendorTags(vTags.get(), names);
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

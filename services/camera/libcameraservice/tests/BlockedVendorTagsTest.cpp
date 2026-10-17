// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "BlockedVendorTagsTest"

#include <vector>

#include <gtest/gtest.h>
#include <system/camera_metadata.h>
#include <system/camera_vendor_tags.h>

#include "../utils/BlockedVendorTags.h"

using namespace android;
using namespace android::camera3;

namespace {

constexpr uint32_t kInsensorZoom = VENDOR_SECTION_START;
constexpr uint32_t kSnapshotOnlyInsensorZoom = VENDOR_SECTION_START + 1;
constexpr uint32_t kMfnr = VENDOR_SECTION_START + 2;
const char* const kTagNames[] = {"EnableInsensorZoom", "EnableSnapshotOnlyInsensorZoom",
        "enableMFNR"};

int getTagCount(const vendor_tag_ops_t*) {
    return 3;
}

void getAllTags(const vendor_tag_ops_t*, uint32_t* tags) {
    for (uint32_t i = 0; i < 3; i++) {
        tags[i] = VENDOR_SECTION_START + i;
    }
}

const char* getSectionName(const vendor_tag_ops_t*, uint32_t) {
    return "org.codeaurora.qcamera3.sessionParameters";
}

const char* getTagName(const vendor_tag_ops_t*, uint32_t tag) {
    return kTagNames[tag - VENDOR_SECTION_START];
}

int getTagType(const vendor_tag_ops_t*, uint32_t) {
    return TYPE_INT32;
}

// The FP6 HAL's session parameter section, with one tag that stays allowed.
sp<VendorTagDescriptor> qcomSessionParameters() {
    vendor_tag_ops_t ops = {};
    ops.get_tag_count = getTagCount;
    ops.get_all_tags = getAllTags;
    ops.get_section_name = getSectionName;
    ops.get_tag_name = getTagName;
    ops.get_tag_type = getTagType;
    sp<VendorTagDescriptor> desc;
    EXPECT_EQ(OK, VendorTagDescriptor::createDescriptorFromOps(&ops, desc));
    return desc;
}

} // namespace

TEST(BlockedVendorTagsTest, ResolvesTheInSensorZoomTags) {
    sp<VendorTagDescriptor> desc = qcomSessionParameters();
    ASSERT_NE(nullptr, desc.get());
    std::vector<uint32_t> expected = {kInsensorZoom, kSnapshotOnlyInsensorZoom};
    EXPECT_EQ(expected, getBlockedVendorTags(desc.get()));
}

TEST(BlockedVendorTagsTest, NothingWithoutTheVendorTags) {
    EXPECT_TRUE(getBlockedVendorTags(nullptr).empty());
}

TEST(BlockedVendorTagsTest, ErasesOnlyTheBlockedTags) {
    sp<VendorTagDescriptor> previous = VendorTagDescriptor::getGlobalVendorTagDescriptor();
    sp<VendorTagDescriptor> desc = qcomSessionParameters();
    ASSERT_EQ(OK, VendorTagDescriptor::setAsGlobalVendorTagDescriptor(desc));

    CameraMetadata settings;
    int32_t on = 1;
    float zoom = 2.0f;
    ASSERT_EQ(OK, settings.update(kInsensorZoom, &on, 1));
    ASSERT_EQ(OK, settings.update(kSnapshotOnlyInsensorZoom, &on, 1));
    ASSERT_EQ(OK, settings.update(kMfnr, &on, 1));
    ASSERT_EQ(OK, settings.update(ANDROID_CONTROL_ZOOM_RATIO, &zoom, 1));

    std::vector<uint32_t> blocked = getBlockedVendorTags(CAMERA_METADATA_INVALID_VENDOR_ID);
    EXPECT_TRUE(eraseTags(settings, blocked));
    EXPECT_FALSE(settings.exists(kInsensorZoom));
    EXPECT_FALSE(settings.exists(kSnapshotOnlyInsensorZoom));
    EXPECT_TRUE(settings.exists(kMfnr));
    EXPECT_TRUE(settings.exists(ANDROID_CONTROL_ZOOM_RATIO));
    EXPECT_FALSE(eraseTags(settings, blocked));

    if (previous != nullptr) {
        VendorTagDescriptor::setAsGlobalVendorTagDescriptor(previous);
    } else {
        VendorTagDescriptor::clearGlobalVendorTagDescriptor();
    }
}

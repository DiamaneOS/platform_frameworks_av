// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "Camera3-MuteUtils"

#include "device3/CameraMuteUtils.h"

#include <algorithm>
#include <cstring>

#include <system/graphics.h>

namespace android {

namespace camera3 {

namespace {

constexpr uint32_t kCurveTags[3] = {
    ANDROID_TONEMAP_CURVE_RED,
    ANDROID_TONEMAP_CURVE_GREEN,
    ANDROID_TONEMAP_CURVE_BLUE,
};

// (in, out) control points: every input maps to 0.
constexpr float kBlackCurve[4] = {0.0f, 0.0f, 1.0f, 0.0f};

bool contains(const camera_metadata_ro_entry& entry, int32_t value) {
    for (size_t i = 0; i < entry.count; i++) {
        if (entry.data.i32[i] == value) return true;
    }
    return false;
}

bool containsU8(const camera_metadata_ro_entry& entry, uint8_t value) {
    for (size_t i = 0; i < entry.count; i++) {
        if (entry.data.u8[i] == value) return true;
    }
    return false;
}

// A curve that maps every input to 0, as set by the override. The HAL may report it with
// more control points than were requested.
template <typename Entry>
bool isBlackCurve(const Entry& entry) {
    if (entry.count < 4 || entry.count % 2 != 0) return false;
    for (size_t i = 1; i < entry.count; i += 2) {
        if (entry.data.f[i] != 0.0f) return false;
    }
    return true;
}

bool setU8(CameraMetadata* metadata, uint32_t tag, uint8_t value) {
    camera_metadata_entry entry = metadata->find(tag);
    if (entry.count == 1 && entry.data.u8[0] == value) return false;
    metadata->update(tag, &value, 1);
    return true;
}

bool setFloats(CameraMetadata* metadata, uint32_t tag, const float* values, size_t count) {
    camera_metadata_entry entry = metadata->find(tag);
    if (entry.count == count && std::equal(values, values + count, entry.data.f)) return false;
    metadata->update(tag, values, count);
    return true;
}

bool eraseTag(CameraMetadata* metadata, uint32_t tag) {
    if (metadata->find(tag).count == 0) return false;
    metadata->erase(tag);
    return true;
}

} // namespace

int32_t getCameraMuteTestPattern(const CameraMetadata& deviceInfo) {
    camera_metadata_ro_entry modes = deviceInfo.find(ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES);
    for (int32_t mode : {ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR,
            ANDROID_SENSOR_TEST_PATTERN_MODE_BLACK}) {
        if (contains(modes, mode)) return mode;
    }
    return ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
}

bool isTestPatternSessionKey(const CameraMetadata& deviceInfo) {
    return contains(deviceInfo.find(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS),
            ANDROID_SENSOR_TEST_PATTERN_MODE);
}

bool supportsCameraMuteTonemapBlanking(const CameraMetadata& deviceInfo) {
    if (!containsU8(deviceInfo.find(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES),
            ANDROID_TONEMAP_MODE_CONTRAST_CURVE)) {
        return false;
    }
    camera_metadata_ro_entry maxPoints = deviceInfo.find(ANDROID_TONEMAP_MAX_CURVE_POINTS);
    if (maxPoints.count == 0 || maxPoints.data.i32[0] < 2) return false;

    camera_metadata_ro_entry requestKeys =
            deviceInfo.find(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS);
    camera_metadata_ro_entry resultKeys =
            deviceInfo.find(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS);
    camera_metadata_ro_entry sessionKeys =
            deviceInfo.find(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS);
    for (int32_t tag : {ANDROID_TONEMAP_MODE, ANDROID_TONEMAP_CURVE_RED,
            ANDROID_TONEMAP_CURVE_GREEN, ANDROID_TONEMAP_CURVE_BLUE}) {
        // Results must report the curve: buffers are released only after the result shows it.
        // A session key change would reconfigure the streams, which is what this avoids.
        if (!contains(requestKeys, tag) || !contains(resultKeys, tag) ||
                contains(sessionKeys, tag)) {
            return false;
        }
    }
    return true;
}

bool isUnprocessedCameraOutput(int format, int32_t dataSpace) {
    switch (format) {
        case HAL_PIXEL_FORMAT_RAW16:
        case HAL_PIXEL_FORMAT_RAW10:
        case HAL_PIXEL_FORMAT_RAW12:
        case HAL_PIXEL_FORMAT_RAW_OPAQUE:
        case HAL_PIXEL_FORMAT_Y16:  // DEPTH16
            return true;
        case HAL_PIXEL_FORMAT_BLOB:
            // DEPTH_POINT_CLOUD; JPEG, HEIC and JPEG_R blobs are processed
            return dataSpace == HAL_DATASPACE_DEPTH;
        default:
            return false;
    }
}

CameraMuteTonemap CameraMuteTonemap::fromSettings(const CameraMetadata& settings) {
    CameraMuteTonemap tonemap;
    camera_metadata_ro_entry mode = settings.find(ANDROID_TONEMAP_MODE);
    if (mode.count > 0) {
        tonemap.hasMode = true;
        tonemap.mode = mode.data.u8[0];
    }
    for (size_t i = 0; i < 3; i++) {
        camera_metadata_ro_entry curve = settings.find(kCurveTags[i]);
        if (curve.count > 0) {
            tonemap.hasCurve[i] = true;
            tonemap.curve[i].assign(curve.data.f, curve.data.f + curve.count);
        }
    }
    return tonemap;
}

CameraMuteTonemap CameraMuteTonemap::withDefaults(const CameraMuteTonemap& defaults) const {
    CameraMuteTonemap merged = *this;
    if (!merged.hasMode && defaults.hasMode) {
        merged.hasMode = true;
        merged.mode = defaults.mode;
    }
    for (size_t i = 0; i < 3; i++) {
        if (!merged.hasCurve[i] && defaults.hasCurve[i]) {
            merged.hasCurve[i] = true;
            merged.curve[i] = defaults.curve[i];
        }
    }
    return merged;
}

bool overrideTonemapForMute(CameraMetadata* settings, bool mute,
        const CameraMuteTonemap& original) {
    bool changed = false;
    if (mute) {
        changed |= setU8(settings, ANDROID_TONEMAP_MODE, ANDROID_TONEMAP_MODE_CONTRAST_CURVE);
        for (uint32_t tag : kCurveTags) {
            changed |= setFloats(settings, tag, kBlackCurve, 4);
        }
        return changed;
    }

    changed |= original.hasMode ? setU8(settings, ANDROID_TONEMAP_MODE, original.mode)
                                : eraseTag(settings, ANDROID_TONEMAP_MODE);
    for (size_t i = 0; i < 3; i++) {
        changed |= original.hasCurve[i]
                ? setFloats(settings, kCurveTags[i], original.curve[i].data(),
                        original.curve[i].size())
                : eraseTag(settings, kCurveTags[i]);
    }
    return changed;
}

bool isTonemapBlankedResult(const CameraMetadata& result) {
    camera_metadata_ro_entry mode = result.find(ANDROID_TONEMAP_MODE);
    if (mode.count != 1 || mode.data.u8[0] != ANDROID_TONEMAP_MODE_CONTRAST_CURVE) return false;
    for (uint32_t tag : kCurveTags) {
        if (!isBlackCurve(result.find(tag))) return false;
    }
    return true;
}

void fixupCameraMuteResult(CameraMetadata* result, const CameraMuteResultFixup& fixup,
        const std::string& physicalCameraId) {
    const CameraMuteResultFixup::TestPattern* testPattern = &fixup.logicalTestPattern;
    if (!physicalCameraId.empty()) {
        auto it = fixup.physicalTestPatterns.find(physicalCameraId);
        if (it != fixup.physicalTestPatterns.end()) testPattern = &it->second;
    }
    camera_metadata_entry mode = result->find(ANDROID_SENSOR_TEST_PATTERN_MODE);
    if (mode.count > 0) {
        mode.data.i32[0] = testPattern->mode;
    }
    camera_metadata_entry data = result->find(ANDROID_SENSOR_TEST_PATTERN_DATA);
    if (data.count >= 4) {
        memcpy(data.data.i32, testPattern->data, sizeof(testPattern->data));
    }

    if (!fixup.tonemapBlanked) return;

    // Only replace what still shows the override; a physical camera with its own tonemap
    // settings was not blanked and reports the app's values already.
    bool blanked = true;
    for (uint32_t tag : kCurveTags) {
        camera_metadata_entry curve = result->find(tag);
        if (curve.count > 0 && !isBlackCurve(curve)) blanked = false;
    }
    camera_metadata_entry tonemapMode = result->find(ANDROID_TONEMAP_MODE);
    if (blanked && tonemapMode.count > 0 &&
            tonemapMode.data.u8[0] == ANDROID_TONEMAP_MODE_CONTRAST_CURVE) {
        // Templates always set a tonemap mode; FAST covers requests built without one.
        tonemapMode.data.u8[0] = fixup.tonemap.hasMode ? fixup.tonemap.mode
                                                       : ANDROID_TONEMAP_MODE_FAST;
    }
    for (size_t i = 0; blanked && i < 3; i++) {
        camera_metadata_entry curve = result->find(kCurveTags[i]);
        if (curve.count == 0) continue;
        if (fixup.tonemap.hasCurve[i]) {
            result->update(kCurveTags[i], fixup.tonemap.curve[i].data(),
                    fixup.tonemap.curve[i].size());
        } else {
            result->erase(kCurveTags[i]);
        }
    }
}

} // namespace camera3

} // namespace android

// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "CameraMuteUtilsTest"

#include <vector>

#include <gtest/gtest.h>
#include <system/graphics.h>

#include "../device3/CameraMuteUtils.h"

using namespace android;
using namespace android::camera3;

namespace {

const std::vector<int32_t> kTonemapKeys = {ANDROID_TONEMAP_MODE, ANDROID_TONEMAP_CURVE_RED,
        ANDROID_TONEMAP_CURVE_GREEN, ANDROID_TONEMAP_CURVE_BLUE};

// Device with the given test patterns, full tonemap curve support and the test pattern mode as
// a session key (the FP6 case).
CameraMetadata deviceInfo(std::vector<int32_t> testPatterns) {
    CameraMetadata info;
    info.update(ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES, testPatterns.data(),
            testPatterns.size());
    uint8_t tonemapModes[] = {ANDROID_TONEMAP_MODE_CONTRAST_CURVE, ANDROID_TONEMAP_MODE_FAST,
            ANDROID_TONEMAP_MODE_HIGH_QUALITY};
    info.update(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES, tonemapModes, 3);
    int32_t maxPoints = 64;
    info.update(ANDROID_TONEMAP_MAX_CURVE_POINTS, &maxPoints, 1);
    info.update(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, kTonemapKeys.data(),
            kTonemapKeys.size());
    info.update(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, kTonemapKeys.data(),
            kTonemapKeys.size());
    int32_t sessionKeys[] = {ANDROID_CONTROL_AE_TARGET_FPS_RANGE, ANDROID_SENSOR_TEST_PATTERN_MODE};
    info.update(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS, sessionKeys, 2);
    return info;
}

std::vector<float> floats(const CameraMetadata& metadata, uint32_t tag) {
    camera_metadata_ro_entry entry = metadata.find(tag);
    return std::vector<float>(entry.data.f, entry.data.f + entry.count);
}

uint8_t u8(const CameraMetadata& metadata, uint32_t tag) {
    camera_metadata_ro_entry entry = metadata.find(tag);
    EXPECT_EQ(entry.count, 1u);
    return entry.count > 0 ? entry.data.u8[0] : 0xff;
}

const std::vector<float> kBlack = {0.0f, 0.0f, 1.0f, 0.0f};
const std::vector<float> kAppCurve = {0.0f, 0.0f, 0.5f, 0.7f, 1.0f, 1.0f};

}  // namespace

TEST(CameraMuteUtilsTest, TestPatternPreference) {
    using P = std::vector<int32_t>;
    EXPECT_EQ(getCameraMuteTestPattern(deviceInfo(P{0, 5, 1})),
            ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR);
    EXPECT_EQ(getCameraMuteTestPattern(deviceInfo(P{0, 5})),
            ANDROID_SENSOR_TEST_PATTERN_MODE_BLACK);
    EXPECT_EQ(getCameraMuteTestPattern(deviceInfo(P{0, 4})), ANDROID_SENSOR_TEST_PATTERN_MODE_OFF);
    EXPECT_EQ(getCameraMuteTestPattern(CameraMetadata()), ANDROID_SENSOR_TEST_PATTERN_MODE_OFF);
}

TEST(CameraMuteUtilsTest, TestPatternSessionKey) {
    CameraMetadata info = deviceInfo({0, 5});
    EXPECT_TRUE(isTestPatternSessionKey(info));
    int32_t fpsOnly = ANDROID_CONTROL_AE_TARGET_FPS_RANGE;
    info.update(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS, &fpsOnly, 1);
    EXPECT_FALSE(isTestPatternSessionKey(info));
    EXPECT_FALSE(isTestPatternSessionKey(CameraMetadata()));
}

TEST(CameraMuteUtilsTest, TonemapBlankingNeedsCurveSupport) {
    EXPECT_TRUE(supportsCameraMuteTonemapBlanking(deviceInfo({0, 5})));

    CameraMetadata noCurveMode = deviceInfo({0, 5});
    uint8_t fastOnly = ANDROID_TONEMAP_MODE_FAST;
    noCurveMode.update(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES, &fastOnly, 1);
    EXPECT_FALSE(supportsCameraMuteTonemapBlanking(noCurveMode));

    CameraMetadata noPoints = deviceInfo({0, 5});
    noPoints.erase(ANDROID_TONEMAP_MAX_CURVE_POINTS);
    EXPECT_FALSE(supportsCameraMuteTonemapBlanking(noPoints));

    int32_t modeOnly = ANDROID_TONEMAP_MODE;
    CameraMetadata noCurveRequestKey = deviceInfo({0, 5});
    noCurveRequestKey.update(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, &modeOnly, 1);
    EXPECT_FALSE(supportsCameraMuteTonemapBlanking(noCurveRequestKey));

    // Results must report the curve, or held buffers could never be released
    CameraMetadata noCurveResultKey = deviceInfo({0, 5});
    noCurveResultKey.update(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, &modeOnly, 1);
    EXPECT_FALSE(supportsCameraMuteTonemapBlanking(noCurveResultKey));

    CameraMetadata tonemapSessionKey = deviceInfo({0, 5});
    tonemapSessionKey.update(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS, &modeOnly, 1);
    EXPECT_FALSE(supportsCameraMuteTonemapBlanking(tonemapSessionKey));
}

TEST(CameraMuteUtilsTest, UnprocessedOutputs) {
    EXPECT_TRUE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_RAW16, HAL_DATASPACE_ARBITRARY));
    EXPECT_TRUE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_RAW10, HAL_DATASPACE_ARBITRARY));
    EXPECT_TRUE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_RAW12, HAL_DATASPACE_ARBITRARY));
    EXPECT_TRUE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_RAW_OPAQUE, HAL_DATASPACE_ARBITRARY));
    EXPECT_TRUE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_Y16, HAL_DATASPACE_DEPTH));
    EXPECT_TRUE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_BLOB, HAL_DATASPACE_DEPTH));
    EXPECT_FALSE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_BLOB, HAL_DATASPACE_V0_JFIF));
    EXPECT_FALSE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_YCBCR_420_888,
            HAL_DATASPACE_V0_JFIF));
    EXPECT_FALSE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
            HAL_DATASPACE_UNKNOWN));
    EXPECT_FALSE(isUnprocessedCameraOutput(HAL_PIXEL_FORMAT_Y8, HAL_DATASPACE_UNKNOWN));
}

TEST(CameraMuteUtilsTest, BlankedResultCheck) {
    CameraMetadata result;
    EXPECT_FALSE(isTonemapBlankedResult(result));

    uint8_t curveMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    result.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    std::vector<float> resampled = {0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f};
    result.update(ANDROID_TONEMAP_CURVE_RED, resampled.data(), resampled.size());
    result.update(ANDROID_TONEMAP_CURVE_GREEN, kBlack.data(), kBlack.size());
    // A missing curve is not proof
    EXPECT_FALSE(isTonemapBlankedResult(result));
    result.update(ANDROID_TONEMAP_CURVE_BLUE, kBlack.data(), kBlack.size());
    EXPECT_TRUE(isTonemapBlankedResult(result));

    // Any output point above 0 fails
    std::vector<float> leak = {0.0f, 0.0f, 1.0f, 0.01f};
    result.update(ANDROID_TONEMAP_CURVE_GREEN, leak.data(), leak.size());
    EXPECT_FALSE(isTonemapBlankedResult(result));
    result.update(ANDROID_TONEMAP_CURVE_GREEN, kBlack.data(), kBlack.size());

    uint8_t fast = ANDROID_TONEMAP_MODE_FAST;
    result.update(ANDROID_TONEMAP_MODE, &fast, 1);
    EXPECT_FALSE(isTonemapBlankedResult(result));
}

TEST(CameraMuteUtilsTest, TonemapStateFailsClosedOnce) {
    CameraMuteTonemapState state;
    EXPECT_FALSE(state.failed());
    EXPECT_FALSE(state.reportResult(true));
    EXPECT_FALSE(state.failed());
    EXPECT_TRUE(state.reportResult(false));
    EXPECT_TRUE(state.failed());
    // Stays failed, reported once
    EXPECT_FALSE(state.reportResult(false));
    EXPECT_FALSE(state.reportResult(true));
    EXPECT_TRUE(state.failed());
}

TEST(CameraMuteUtilsTest, BlankAndRestoreAppTonemap) {
    CameraMetadata settings;
    uint8_t fast = ANDROID_TONEMAP_MODE_FAST;
    settings.update(ANDROID_TONEMAP_MODE, &fast, 1);
    CameraMuteTonemap original = CameraMuteTonemap::fromSettings(settings);

    EXPECT_TRUE(overrideTonemapForMute(&settings, true, original));
    EXPECT_EQ(u8(settings, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_CONTRAST_CURVE);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_RED), kBlack);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_GREEN), kBlack);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_BLUE), kBlack);
    // Already blanked: no change, so a repeating request isn't resent
    EXPECT_FALSE(overrideTonemapForMute(&settings, true, original));

    EXPECT_TRUE(overrideTonemapForMute(&settings, false, original));
    EXPECT_EQ(u8(settings, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_FAST);
    EXPECT_EQ(settings.find(ANDROID_TONEMAP_CURVE_RED).count, 0u);
    EXPECT_EQ(settings.find(ANDROID_TONEMAP_CURVE_GREEN).count, 0u);
    EXPECT_EQ(settings.find(ANDROID_TONEMAP_CURVE_BLUE).count, 0u);
    EXPECT_FALSE(overrideTonemapForMute(&settings, false, original));
}

TEST(CameraMuteUtilsTest, RestoreAppContrastCurve) {
    CameraMetadata settings;
    uint8_t curveMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    settings.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    settings.update(ANDROID_TONEMAP_CURVE_RED, kAppCurve.data(), kAppCurve.size());
    settings.update(ANDROID_TONEMAP_CURVE_GREEN, kAppCurve.data(), kAppCurve.size());
    settings.update(ANDROID_TONEMAP_CURVE_BLUE, kAppCurve.data(), kAppCurve.size());
    CameraMuteTonemap original = CameraMuteTonemap::fromSettings(settings);

    EXPECT_TRUE(overrideTonemapForMute(&settings, true, original));
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_GREEN), kBlack);
    EXPECT_TRUE(overrideTonemapForMute(&settings, false, original));
    EXPECT_EQ(u8(settings, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_CONTRAST_CURVE);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_RED), kAppCurve);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_GREEN), kAppCurve);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_BLUE), kAppCurve);
}

TEST(CameraMuteUtilsTest, ResultShowsAppValues) {
    CameraMetadata request;
    uint8_t hq = ANDROID_TONEMAP_MODE_HIGH_QUALITY;
    request.update(ANDROID_TONEMAP_MODE, &hq, 1);

    CameraMuteResultFixup fixup;
    fixup.tonemapBlanked = true;
    fixup.tonemap = CameraMuteTonemap::fromSettings(request);

    // What the HAL echoes for a muted capture; it may resample the curve
    CameraMetadata result;
    int32_t black = ANDROID_SENSOR_TEST_PATTERN_MODE_BLACK;
    result.update(ANDROID_SENSOR_TEST_PATTERN_MODE, &black, 1);
    int32_t data[4] = {1, 2, 3, 4};
    result.update(ANDROID_SENSOR_TEST_PATTERN_DATA, data, 4);
    uint8_t curveMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    result.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    std::vector<float> resampled = {0.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f};
    result.update(ANDROID_TONEMAP_CURVE_RED, resampled.data(), resampled.size());
    result.update(ANDROID_TONEMAP_CURVE_GREEN, resampled.data(), resampled.size());
    result.update(ANDROID_TONEMAP_CURVE_BLUE, resampled.data(), resampled.size());

    fixupCameraMuteResult(&result, fixup);

    camera_metadata_entry mode = result.find(ANDROID_SENSOR_TEST_PATTERN_MODE);
    ASSERT_EQ(mode.count, 1u);
    EXPECT_EQ(mode.data.i32[0], ANDROID_SENSOR_TEST_PATTERN_MODE_OFF);
    camera_metadata_entry resultData = result.find(ANDROID_SENSOR_TEST_PATTERN_DATA);
    ASSERT_EQ(resultData.count, 4u);
    for (size_t i = 0; i < 4; i++) EXPECT_EQ(resultData.data.i32[i], 0);
    EXPECT_EQ(u8(result, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_HIGH_QUALITY);
    EXPECT_EQ(result.find(ANDROID_TONEMAP_CURVE_RED).count, 0u);
    EXPECT_EQ(result.find(ANDROID_TONEMAP_CURVE_GREEN).count, 0u);
    EXPECT_EQ(result.find(ANDROID_TONEMAP_CURVE_BLUE).count, 0u);
}

TEST(CameraMuteUtilsTest, ResultRestoresAppCurveAndPattern) {
    CameraMetadata request;
    uint8_t curveMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    request.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    request.update(ANDROID_TONEMAP_CURVE_RED, kAppCurve.data(), kAppCurve.size());

    CameraMuteResultFixup fixup;
    fixup.logicalTestPattern.mode = ANDROID_SENSOR_TEST_PATTERN_MODE_COLOR_BARS;
    fixup.tonemapBlanked = true;
    fixup.tonemap = CameraMuteTonemap::fromSettings(request);

    CameraMetadata result;
    int32_t black = ANDROID_SENSOR_TEST_PATTERN_MODE_BLACK;
    result.update(ANDROID_SENSOR_TEST_PATTERN_MODE, &black, 1);
    result.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    result.update(ANDROID_TONEMAP_CURVE_RED, kBlack.data(), kBlack.size());

    fixupCameraMuteResult(&result, fixup);

    EXPECT_EQ(result.find(ANDROID_SENSOR_TEST_PATTERN_MODE).data.i32[0],
            ANDROID_SENSOR_TEST_PATTERN_MODE_COLOR_BARS);
    EXPECT_EQ(u8(result, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_CONTRAST_CURVE);
    EXPECT_EQ(floats(result, ANDROID_TONEMAP_CURVE_RED), kAppCurve);
}

TEST(CameraMuteUtilsTest, PhysicalResults) {
    CameraMuteResultFixup fixup;
    fixup.logicalTestPattern.mode = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    CameraMuteResultFixup::TestPattern physical;
    physical.mode = ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR;
    physical.data[1] = 7;
    fixup.physicalTestPatterns["2"] = physical;
    fixup.tonemapBlanked = true;

    int32_t black = ANDROID_SENSOR_TEST_PATTERN_MODE_BLACK;
    CameraMetadata withSettings;
    withSettings.update(ANDROID_SENSOR_TEST_PATTERN_MODE, &black, 1);
    int32_t zeros[4] = {};
    withSettings.update(ANDROID_SENSOR_TEST_PATTERN_DATA, zeros, 4);
    fixupCameraMuteResult(&withSettings, fixup, "2");
    EXPECT_EQ(withSettings.find(ANDROID_SENSOR_TEST_PATTERN_MODE).data.i32[0],
            ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR);
    EXPECT_EQ(withSettings.find(ANDROID_SENSOR_TEST_PATTERN_DATA).data.i32[1], 7);

    // No physical settings: falls back to the logical camera's values
    CameraMetadata withoutSettings;
    withoutSettings.update(ANDROID_SENSOR_TEST_PATTERN_MODE, &black, 1);
    fixupCameraMuteResult(&withoutSettings, fixup, "3");
    EXPECT_EQ(withoutSettings.find(ANDROID_SENSOR_TEST_PATTERN_MODE).data.i32[0],
            ANDROID_SENSOR_TEST_PATTERN_MODE_OFF);

    // A tonemap that isn't the override is left alone
    CameraMetadata ownTonemap;
    uint8_t curveMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    ownTonemap.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    ownTonemap.update(ANDROID_TONEMAP_CURVE_RED, kAppCurve.data(), kAppCurve.size());
    fixupCameraMuteResult(&ownTonemap, fixup, "2");
    EXPECT_EQ(u8(ownTonemap, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_CONTRAST_CURVE);
    EXPECT_EQ(floats(ownTonemap, ANDROID_TONEMAP_CURVE_RED), kAppCurve);
}

TEST(CameraMuteUtilsTest, ResultWithoutBlankingKeepsTonemap) {
    CameraMuteResultFixup fixup;  // SOLID_COLOR/BLACK devices: no tonemap blanking
    CameraMetadata result;
    uint8_t curveMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    result.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    result.update(ANDROID_TONEMAP_CURVE_RED, kBlack.data(), kBlack.size());
    int32_t solid = ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR;
    result.update(ANDROID_SENSOR_TEST_PATTERN_MODE, &solid, 1);

    fixupCameraMuteResult(&result, fixup);

    EXPECT_EQ(result.find(ANDROID_SENSOR_TEST_PATTERN_MODE).data.i32[0],
            ANDROID_SENSOR_TEST_PATTERN_MODE_OFF);
    EXPECT_EQ(u8(result, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_CONTRAST_CURVE);
    EXPECT_EQ(floats(result, ANDROID_TONEMAP_CURVE_RED), kBlack);
}

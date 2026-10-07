// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "CameraMuteUtilsTest"

#include <cmath>
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

TEST(CameraMuteUtilsTest, RestoreWritesDefaultCurvesWhereTheAppHasNone) {
    // A HAL whose template has curves: the app's request has a mode but no curves, and
    // unmuting writes the template's curves rather than removing the zero curve's keys (a HAL
    // that keeps the last curves would otherwise stay on a linear, darker curve).
    CameraMetadata templ;
    uint8_t fast = ANDROID_TONEMAP_MODE_FAST;
    templ.update(ANDROID_TONEMAP_MODE, &fast, 1);
    templ.update(ANDROID_TONEMAP_CURVE_RED, kAppCurve.data(), kAppCurve.size());
    templ.update(ANDROID_TONEMAP_CURVE_GREEN, kAppCurve.data(), kAppCurve.size());
    templ.update(ANDROID_TONEMAP_CURVE_BLUE, kAppCurve.data(), kAppCurve.size());
    CameraMuteTonemap defaults = CameraMuteTonemap::fromSettings(templ);

    CameraMetadata settings;
    uint8_t hq = ANDROID_TONEMAP_MODE_HIGH_QUALITY;
    settings.update(ANDROID_TONEMAP_MODE, &hq, 1);
    CameraMuteTonemap restore = CameraMuteTonemap::fromSettings(settings).withDefaults(defaults);
    EXPECT_TRUE(restore.hasMode);
    EXPECT_EQ(restore.mode, ANDROID_TONEMAP_MODE_HIGH_QUALITY);  // the app's mode wins

    EXPECT_TRUE(overrideTonemapForMute(&settings, true, restore));
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_RED), kBlack);
    EXPECT_TRUE(overrideTonemapForMute(&settings, false, restore));
    EXPECT_EQ(u8(settings, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_HIGH_QUALITY);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_RED), kAppCurve);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_GREEN), kAppCurve);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_BLUE), kAppCurve);

    // Muted results show the template's curves, not missing keys
    CameraMuteResultFixup fixup;
    fixup.tonemapBlanked = true;
    fixup.tonemap = restore;
    CameraMetadata result;
    uint8_t curveMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    result.update(ANDROID_TONEMAP_MODE, &curveMode, 1);
    result.update(ANDROID_TONEMAP_CURVE_RED, kBlack.data(), kBlack.size());
    fixupCameraMuteResult(&result, fixup);
    EXPECT_EQ(u8(result, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_HIGH_QUALITY);
    EXPECT_EQ(floats(result, ANDROID_TONEMAP_CURVE_RED), kAppCurve);
}

namespace {

// A 32-point gamma-like curve as the FP6 reports in FAST mode (inputs i/31).
std::vector<float> fp6LiveCurve() {
    std::vector<float> curve;
    for (int i = 0; i < 32; i++) {
        float x = i / 31.0f;
        curve.push_back(x);
        curve.push_back(std::pow(x, 0.45f));
    }
    return curve;
}

CameraMetadata liveResult(uint8_t mode, const std::vector<float>& curve) {
    CameraMetadata result;
    result.update(ANDROID_TONEMAP_MODE, &mode, 1);
    result.update(ANDROID_TONEMAP_CURVE_RED, curve.data(), curve.size());
    result.update(ANDROID_TONEMAP_CURVE_GREEN, curve.data(), curve.size());
    result.update(ANDROID_TONEMAP_CURVE_BLUE, curve.data(), curve.size());
    return result;
}

bool report(CameraMuteTonemapState* state, CameraMetadata& result) {
    const camera_metadata_t* raw = result.getAndLock();
    bool kept = state->reportLiveResult(raw);
    result.unlock(raw);
    return kept;
}

}  // namespace

TEST(CameraMuteUtilsTest, Fp6RestoreUsesTheLiveCurvesWhenTheTemplateHasNone) {
    // The FP6's preview template has a tonemap mode but no curves, as has the app's request.
    CameraMetadata templ;
    uint8_t fast = ANDROID_TONEMAP_MODE_FAST;
    templ.update(ANDROID_TONEMAP_MODE, &fast, 1);
    CameraMuteTonemap templateTonemap = CameraMuteTonemap::fromSettings(templ);
    EXPECT_FALSE(templateTonemap.hasCurve[0]);

    CameraMuteTonemapState state(32);
    EXPECT_FALSE(state.hasLiveCurves());
    // Before any live result: an evenly spaced 32-point grid (the FP6's own input points)
    CameraMuteTonemap grid = state.restoreCurves();
    ASSERT_EQ(grid.curve[0].size(), 64u);
    EXPECT_FLOAT_EQ(grid.curve[0][2], 1 / 31.0f);
    EXPECT_FLOAT_EQ(grid.curve[0][63], 1.0f);

    // Results that must not be kept: the zero curve, the 2-point curve left by an unmute that
    // removed the keys, and contrast-curve mode
    CameraMetadata zero = liveResult(ANDROID_TONEMAP_MODE_FAST, kBlack);
    EXPECT_FALSE(report(&state, zero));
    std::vector<float> stale = {0.0f, 0.0f, 1.0f, 0.997f};
    CameraMetadata twoPoints = liveResult(ANDROID_TONEMAP_MODE_FAST, stale);
    EXPECT_FALSE(report(&state, twoPoints));
    CameraMetadata manual = liveResult(ANDROID_TONEMAP_MODE_CONTRAST_CURVE, fp6LiveCurve());
    EXPECT_FALSE(report(&state, manual));
    EXPECT_FALSE(state.hasLiveCurves());

    CameraMetadata live = liveResult(ANDROID_TONEMAP_MODE_FAST, fp6LiveCurve());
    EXPECT_TRUE(report(&state, live));
    EXPECT_TRUE(state.hasLiveCurves());
    // Only the first is kept
    std::vector<float> other = fp6LiveCurve();
    other[3] = 0.5f;
    CameraMetadata later = liveResult(ANDROID_TONEMAP_MODE_FAST, other);
    EXPECT_FALSE(report(&state, later));

    CameraMetadata settings;
    settings.update(ANDROID_TONEMAP_MODE, &fast, 1);
    CameraMuteTonemap restore = CameraMuteTonemap::fromSettings(settings)
            .withDefaults(templateTonemap).withDefaults(state.restoreCurves());
    EXPECT_TRUE(overrideTonemapForMute(&settings, true, restore));
    EXPECT_TRUE(overrideTonemapForMute(&settings, false, restore));
    EXPECT_EQ(u8(settings, ANDROID_TONEMAP_MODE), ANDROID_TONEMAP_MODE_FAST);
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_RED), fp6LiveCurve());
    EXPECT_EQ(floats(settings, ANDROID_TONEMAP_CURVE_BLUE), fp6LiveCurve());
}

TEST(CameraMuteUtilsTest, WithDefaultsKeepsTheAppsOwnValues) {
    CameraMuteTonemap app;
    app.hasCurve[1] = true;
    app.curve[1] = kBlack;
    CameraMuteTonemap defaults;
    defaults.hasMode = true;
    defaults.mode = ANDROID_TONEMAP_MODE_FAST;
    for (size_t i = 0; i < 3; i++) {
        defaults.hasCurve[i] = true;
        defaults.curve[i] = kAppCurve;
    }
    CameraMuteTonemap merged = app.withDefaults(defaults);
    EXPECT_TRUE(merged.hasMode);
    EXPECT_EQ(merged.mode, ANDROID_TONEMAP_MODE_FAST);
    EXPECT_EQ(merged.curve[0], kAppCurve);
    EXPECT_EQ(merged.curve[1], kBlack);
    EXPECT_EQ(merged.curve[2], kAppCurve);
    // No defaults: unchanged
    CameraMuteTonemap none = app.withDefaults(CameraMuteTonemap());
    EXPECT_FALSE(none.hasMode);
    EXPECT_FALSE(none.hasCurve[0]);
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

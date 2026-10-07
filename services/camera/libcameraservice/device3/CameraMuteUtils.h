// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#ifndef ANDROID_SERVERS_CAMERA3_CAMERAMUTEUTILS_H
#define ANDROID_SERVERS_CAMERA3_CAMERAMUTEUTILS_H

#include <atomic>
#include <mutex>
#include <map>
#include <string>
#include <vector>

#include <camera/CameraMetadata.h>

namespace android {

namespace camera3 {

// Camera mute (camera privacy toggle) helpers.
//
// AOSP mutes a camera by writing the SOLID_COLOR or BLACK sensor test pattern into every
// request. Some HALs list the test pattern mode as a session key: every mute and unmute then
// reconfigures the streams, and the FP6's CamX HAL can hang after such reconfigurations. On those
// HALs, sessions with only processed outputs are muted with an all-zero tonemap curve instead,
// which is a per-request control: the outputs turn black without a reconfiguration. Outputs the
// curve cannot blank (RAW, depth) keep the test-pattern mute. A muted frame's buffers reach the
// app only after its result shows the curve was applied; otherwise they are dropped and the
// device falls back to the test-pattern mute (fail closed). Results of muted captures carry the
// app's own values, so the override does not show in metadata.

// The test pattern used to mute (SOLID_COLOR, else BLACK), or OFF if the device has neither.
int32_t getCameraMuteTestPattern(const CameraMetadata& deviceInfo);

// Whether the HAL lists the test pattern mode as a session key.
bool isTestPatternSessionKey(const CameraMetadata& deviceInfo);

// Whether the HAL supports an all-zero contrast curve as a per-request control: the mode is
// listed, the tonemap mode and curves are request keys and none is a session key.
bool supportsCameraMuteTonemapBlanking(const CameraMetadata& deviceInfo);

// Whether an output stream carries data the tonemap does not process (RAW, depth).
bool isUnprocessedCameraOutput(int format, int32_t dataSpace);

// Tonemap values of a request, saved before the mute override changes them.
struct CameraMuteTonemap {
    bool hasMode = false;
    uint8_t mode = 0;
    bool hasCurve[3] = {};
    std::vector<float> curve[3];

    static CameraMuteTonemap fromSettings(const CameraMetadata& settings);

    // This tonemap with the mode and curves it lacks taken from defaults.
    CameraMuteTonemap withDefaults(const CameraMuteTonemap& defaults) const;
};

// Sets an all-zero tonemap curve (mute) or restores the original tonemap values. Returns
// whether settings changed. Some HALs (FP6 CamX) keep request values that a later request
// omits, and in FAST/HIGH_QUALITY mode sample their own curve at the input points of the
// last curve they received: restore with full curves (withDefaults and
// CameraMuteTonemapState::restoreCurves), not by removing the keys, or the picture stays
// darker after unmuting.
bool overrideTonemapForMute(CameraMetadata* settings, bool mute,
        const CameraMuteTonemap& original);

// Whether a capture result shows the all-zero curve applied: contrast curve mode and all three
// curves present with every output point at 0.
bool isTonemapBlankedResult(const CameraMetadata& result);

// The curves a HAL reports in a live (unmuted, FAST or HIGH_QUALITY) result: all three present
// with at least 3 control points each and not the zero curve. Empty if the result has none.
CameraMuteTonemap liveTonemapCurves(const camera_metadata_t* result);

// Whether a result shows a restore curve applied: CONTRAST_CURVE mode with all three curves of
// at least 3 control points, none of them the zero curve.
bool isTonemapRestoredResult(const camera_metadata_t* result);

// Evenly spaced control points (in = out) with the given number of points per curve.
CameraMuteTonemap gridTonemapCurves(size_t points);

// Per-device camera mute tonemap state, shared by the request thread and in-flight requests.
// Fails closed and stays failed: one muted result without the curve switches the device to the
// test-pattern mute until it is closed. Also keeps the first live curves the HAL reports, to
// restore after unmuting where the app's request and the HAL's template have none.
class CameraMuteTonemapState {
  public:
    explicit CameraMuteTonemapState(size_t gridPoints = 32) : mGridPoints(gridPoints) {}

    bool failed() const { return mFailed.load(); }
    // Returns true if this report turned the state to failed.
    bool reportResult(bool blanked) { return !blanked && !mFailed.exchange(true); }

    bool hasLiveCurves() const { return mHasLiveCurves.load(); }
    // Keeps the result's live curves if none are kept yet. Returns true if it kept them.
    bool reportLiveResult(const camera_metadata_t* result);
    // Curves to restore: the kept live curves, else an evenly spaced grid.
    CameraMuteTonemap restoreCurves() const;

    // The FP6 HAL takes curve control points only in CONTRAST_CURVE mode and keeps the last
    // ones it got in FAST/HIGH_QUALITY. After unmuting, the live curves are sent in
    // CONTRAST_CURVE mode until a result shows them applied (restore phase).
    void beginRestore() { mRestoreConfirmed = false; }
    // Returns true if this report confirmed the restore.
    bool confirmRestore() { return !mRestoreConfirmed.exchange(true); }
    bool restoreConfirmed() const { return mRestoreConfirmed.load(); }

  private:
    std::atomic<bool> mFailed = false;
    std::atomic<bool> mHasLiveCurves = false;
    std::atomic<bool> mRestoreConfirmed = true;
    const size_t mGridPoints;
    mutable std::mutex mLock;
    CameraMuteTonemap mLiveCurves;  // guarded by mLock
};

// App request values to restore in the results of a muted capture.
struct CameraMuteResultFixup {
    struct TestPattern {
        int32_t mode = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
        int32_t data[4] = {};
    };
    TestPattern logicalTestPattern;
    // Physical cameras with their own request settings; others use logicalTestPattern.
    std::map<std::string, TestPattern> physicalTestPatterns;
    // Set when the frame is muted with the tonemap instead of the test pattern.
    bool tonemapBlanked = false;
    // Set for frames of the restore phase after unmuting (CONTRAST_CURVE with live curves).
    bool tonemapRestoring = false;
    CameraMuteTonemap tonemap;
};

// Restores the app's test pattern and tonemap values in a muted capture's result.
// physicalCameraId is empty for the logical camera's result.
void fixupCameraMuteResult(CameraMetadata* result, const CameraMuteResultFixup& fixup,
        const std::string& physicalCameraId = std::string());

} // namespace camera3

} // namespace android

#endif

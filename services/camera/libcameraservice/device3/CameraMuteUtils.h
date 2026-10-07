// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#ifndef ANDROID_SERVERS_CAMERA3_CAMERAMUTEUTILS_H
#define ANDROID_SERVERS_CAMERA3_CAMERAMUTEUTILS_H

#include <atomic>
#include <mutex>
#include <map>
#include <memory>
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

// Evenly spaced inputs with the given number of points per curve, outputs on the Rec. 709
// transfer curve: a display-like curve for when nothing better is known.
CameraMuteTonemap gridTonemapCurves(size_t points);

// Whether two tonemaps have the same curves (each output within 1/1000).
bool sameTonemapCurves(const CameraMuteTonemap& a, const CameraMuteTonemap& b);

// Per-device camera mute tonemap state, shared by the request thread and in-flight requests.
// Fails closed and stays failed: one muted result without the curve switches the device to the
// test-pattern mute until it is closed. Also keeps the HAL's own live curves, to restore after
// unmuting where the app's request and the HAL's template have none.
class CameraMuteTonemapState {
  public:
    // Live frames checked for the HAL's own curves before giving up until the next restore
    static constexpr int kMaxLiveCurveFrames = 60;

    explicit CameraMuteTonemapState(size_t gridPoints = 32) : mGridPoints(gridPoints) {}

    bool failed() const { return mFailed.load(); }
    // Returns true if this report turned the state to failed.
    bool reportResult(bool blanked) { return !blanked && !mFailed.exchange(true); }

    bool hasLiveCurves() const { return mHasLiveCurves.load(); }
    // Whether live results should be checked for the HAL's own curves: at open and after each
    // restore, for up to kMaxLiveCurveFrames frames.
    bool wantsLiveCurves() const { return mWantLiveFrames.load() > 0; }
    // Keeps the result's live curves (FAST/HIGH_QUALITY, full curves) unless they repeat a curve
    // sent by the mute (the last restore curves, the kept curves or the grid): those are echoes,
    // not the HAL's own.
    // The caller must only pass results of frames sent without a mute or restore override.
    // Returns true if it kept them.
    bool reportLiveResult(const camera_metadata_t* result);
    // Curves to restore: the kept live curves, else the grid.
    CameraMuteTonemap restoreCurves() const;

    // The FP6 HAL takes curve control points only in CONTRAST_CURVE mode and keeps the last
    // ones it got in FAST/HIGH_QUALITY. After unmuting, the restore curves are sent in
    // CONTRAST_CURVE mode until a result shows them applied (restore phase).
    void beginRestore(const CameraMuteTonemap& restoreCurves) {
        std::lock_guard<std::mutex> l(mLock);
        mLastRestore = restoreCurves;
        mRestoreConfirmed = false;
    }
    // Returns true if this report confirmed the restore.
    bool confirmRestore() { return !mRestoreConfirmed.exchange(true); }
    bool restoreConfirmed() const { return mRestoreConfirmed.load(); }
    // After a restore: look for the HAL's own live curves again.
    void endRestore() { mWantLiveFrames = kMaxLiveCurveFrames; }

  private:
    std::atomic<bool> mFailed = false;
    std::atomic<bool> mHasLiveCurves = false;
    std::atomic<bool> mRestoreConfirmed = true;
    std::atomic<int> mWantLiveFrames = kMaxLiveCurveFrames;
    const size_t mGridPoints;
    mutable std::mutex mLock;
    CameraMuteTonemap mLiveCurves;  // guarded by mLock
    CameraMuteTonemap mLastRestore;  // guarded by mLock
};

// Request-thread side of the tonemap mute: mutes, then restores after unmuting with a
// CONTRAST_CURVE restore phase. Owned and called by one thread (the request thread).
class CameraMuteTonemapRequests {
  public:
    // About 1 s at 30 fps; then the app's mode with the restore curves
    static constexpr int kMaxRestoreRequests = 30;

    CameraMuteTonemapRequests(std::shared_ptr<CameraMuteTonemapState> state,
            const CameraMuteTonemap& templateTonemap)
        : mState(std::move(state)), mTemplate(templateTonemap) {}

    struct Outcome {
        bool changed = false;    // settings changed
        bool restoring = false;  // the request carries the restore-phase override
    };

    // Applies the mute (mute true) or the restore to one request's logical settings.
    // original: the app's tonemap values; overridden: per-request flag, true while the
    // settings carry an override (in/out).
    Outcome apply(CameraMetadata* settings, const CameraMuteTonemap& original, bool mute,
            bool* overridden);

    // The app's values completed with the template's, the live or the grid curves.
    CameraMuteTonemap restoreTonemap(const CameraMuteTonemap& original) const;

  private:
    std::shared_ptr<CameraMuteTonemapState> mState;
    CameraMuteTonemap mTemplate;
    bool mCurveSent = false;   // zero curve sent; restore on the next unmuted request
    bool mRestoring = false;   // restore phase
    int mRestoreRequests = 0;
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

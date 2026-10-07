// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#ifndef ANDROID_SERVERS_CAMERA3_CAMERAMUTEUTILS_H
#define ANDROID_SERVERS_CAMERA3_CAMERAMUTEUTILS_H

#include <atomic>
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
};

// Sets an all-zero tonemap curve (mute) or restores the original tonemap values. Returns
// whether settings changed.
bool overrideTonemapForMute(CameraMetadata* settings, bool mute,
        const CameraMuteTonemap& original);

// Whether a capture result shows the all-zero curve applied: contrast curve mode and all three
// curves present with every output point at 0.
bool isTonemapBlankedResult(const CameraMetadata& result);

// Whether tonemap blanking still works on this device. Fails closed and stays failed: one muted
// result without the curve switches the device to the test-pattern mute until it is closed.
class CameraMuteTonemapState {
  public:
    bool failed() const { return mFailed.load(); }
    // Returns true if this report turned the state to failed.
    bool reportResult(bool blanked) { return !blanked && !mFailed.exchange(true); }

  private:
    std::atomic<bool> mFailed = false;
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
    CameraMuteTonemap tonemap;
};

// Restores the app's test pattern and tonemap values in a muted capture's result.
// physicalCameraId is empty for the logical camera's result.
void fixupCameraMuteResult(CameraMetadata* result, const CameraMuteResultFixup& fixup,
        const std::string& physicalCameraId = std::string());

} // namespace camera3

} // namespace android

#endif

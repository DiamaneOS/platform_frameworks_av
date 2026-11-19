// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#ifndef ANDROID_SERVERS_CAMERA_PIXELMODEEXEMPTION_H
#define ANDROID_SERVERS_CAMERA_PIXELMODEEXEMPTION_H

#include <sys/types.h>

namespace android {

namespace camera3 {

// Whether the client's app is one its device exempts from the check that a capture request's sensor
// pixel mode matches its streams (ro.camera.pixel_mode_exempt_apps, DeviceCameraLists.h). Its
// package must own the uid and be signed with the listed certificate. Without the property, nothing
// is asked of the package manager.
bool isPixelModeCheckExempt(uid_t uid);

} // namespace camera3

} // namespace android

#endif

// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#define LOG_TAG "PixelModeExemption"

#include "utils/PixelModeExemption.h"

#include <string>
#include <vector>

#include <android/content/pm/IPackageManagerNative.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>

#include "utils/DeviceCameraLists.h"

namespace android {

namespace camera3 {

bool isPixelModeCheckExempt(uid_t uid) {
    std::vector<AppIdentity> apps = pixelModeExemptApps();
    if (apps.empty()) {
        return false;
    }
    sp<IBinder> binder = defaultServiceManager()->checkService(String16("package_native"));
    if (binder == nullptr) {
        return false;
    }
    sp<content::pm::IPackageManagerNative> packageManager =
            interface_cast<content::pm::IPackageManagerNative>(binder);
    std::vector<std::string> names;
    if (!packageManager->getNamesForUids({static_cast<int32_t>(uid)}, &names).isOk() ||
            names.size() != 1) {
        return false;
    }
    for (const AppIdentity& app : apps) {
        if (names[0] != app.package) {
            continue;
        }
        bool signedWithCertificate = false;
        return packageManager->hasSha256SigningCertificate(app.package, app.certificateSha256,
                &signedWithCertificate).isOk() && signedWithCertificate;
    }
    return false;
}

} // namespace camera3

} // namespace android

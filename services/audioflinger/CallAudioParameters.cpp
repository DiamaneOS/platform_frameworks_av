// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include "CallAudioParameters.h"

#include <binder/IPCThreadState.h>
#include <binder/PermissionController.h>
#include <cutils/multiuser.h>
#include <private/android_filesystem_config.h>
#include <utils/String16.h>
#include <utils/Vector.h>

namespace android {

// Vendor call-control parameters affect voice sessions, including emergency calls.
// The normal MODIFY_AUDIO_SETTINGS permission is insufficient for these keys.
// Other audio parameters retain the platform's existing permission/filter policy.
static bool canControlCallAudio() {
    const auto* caller = IPCThreadState::self();
    const uid_t uid = caller->getCallingUid();
    if (uid == AID_ROOT || uid == AID_SYSTEM || uid == AID_RADIO || uid == AID_AUDIOSERVER) {
        return true;
    }
    // This package is never shared-UID and starts only in the primary user.
    // Read live package state. The audio permission provider caches UID packages.
    if (uid >= AID_USER_OFFSET) return false;
    PermissionController permissions;
    static const String16 bridge("de.diamaneos.callaudio");
    Vector<String16> packages;
    permissions.getPackagesForUid(uid, packages);
    if (packages.size() != 1 || packages[0] != bridge) return false;
    static const String16 permission("android.permission.DIAMANEOS_CONTROL_CALL_AUDIO");
    // Signature/privileged ownership is enforced by the platform. Do not retain
    // a grant/denial beyond this transaction or authorize other default signers.
    if (!permissions.checkPermission(permission, caller->getCallingPid(), uid)) return false;
    packages.clear();
    permissions.getPackagesForUid(uid, packages);
    return packages.size() == 1 && packages[0] == bridge;
}

// TTY, hearing-aid, voice-quality and Bluetooth voice-link keys also change a live call.
// Platform services own them: AudioService and Telecom run as system, Settings as system in
// any user, the phone process as radio and the Bluetooth stack as bluetooth.
bool canUseCallAudioParameters(const String8& keyValuePairs) {
    const unsigned groups = callAudioParameterGroups(keyValuePairs);
    if (groups == 0) return true;
    const uid_t uid = IPCThreadState::self()->getCallingUid();
    const uid_t appId = multiuser_get_app_id(uid);
    const bool platform = uid == AID_ROOT || uid == AID_AUDIOSERVER
            || appId == AID_SYSTEM || appId == AID_RADIO;
    if ((groups & kCallAudioFeatureKeys) && !platform) return false;
    if ((groups & kCallAudioBluetoothKeys) && !platform && appId != AID_BLUETOOTH) return false;
    return (groups & kCallAudioSessionKeys) == 0 || canControlCallAudio();
}

} // namespace android

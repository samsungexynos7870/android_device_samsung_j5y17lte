/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>
#include <log/log.h>
#include <math.h>

#include "FmRadioController_brcm.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FMLIB_BRCM_JNI"

static FmRadioController_brcm *pFMRadio = nullptr;

jboolean openDev(JNIEnv *env __unused, jobject thiz __unused)
{
    if (pFMRadio == nullptr)
        pFMRadio = new FmRadioController_brcm();

    if (pFMRadio->Initialise() == 0) {
        ALOGI("%s FM Radio Initialized Successfully", __func__);
        return JNI_TRUE;
    }

    ALOGE("%s failed to initialise Broadcom FmRadio", __func__);
    return JNI_FALSE;
}

jboolean closeDev(JNIEnv *env __unused, jobject thiz __unused)
{
    delete pFMRadio;
    pFMRadio = nullptr;
    ALOGI("%s FM Radio Un-Initialized Successfully", __func__);
    return JNI_TRUE;
}

jboolean powerUp(JNIEnv *env __unused, jobject thiz __unused, jfloat freq)
{
    if (pFMRadio == nullptr)
        return JNI_FALSE;
    pFMRadio->TuneChannel(static_cast<long>(freq * 1000.0f));
    ALOGI("%s [freq=%f]", __func__, freq);
    return JNI_TRUE;
}

jboolean powerDown(JNIEnv *env __unused, jobject thiz __unused, jint type __unused)
{
    ALOGI("%s", __func__);
    return JNI_TRUE;
}

jboolean tune(JNIEnv *env __unused, jobject thiz __unused, jfloat freq)
{
    if (pFMRadio == nullptr)
        return JNI_FALSE;
    pFMRadio->TuneChannel(static_cast<long>(freq * 1000.0f));
    ALOGI("%s [freq=%f]", __func__, freq);
    return JNI_TRUE;
}

jfloat seek(JNIEnv *env __unused, jobject thiz __unused, jfloat freq __unused, jboolean isUp)
{
    if (pFMRadio == nullptr)
        return freq;
    long ret = (isUp == JNI_TRUE) ? pFMRadio->SeekUp() : pFMRadio->SeekDown();
    ALOGI("%s [freq=%ld]", __func__, ret);
    return roundf((ret / 1000.00F) * 100) / 100;
}

jshortArray autoScan(JNIEnv *env, jobject thiz __unused)
{
    if (pFMRadio == nullptr)
        return nullptr;

    const int kMax = 200;
    uint16_t tbl[kMax];
    int cnt = pFMRadio->AutoScan(tbl, kMax);
    if (cnt <= 0)
        return nullptr;

    jshortArray arr = env->NewShortArray(cnt);
    env->SetShortArrayRegion(arr, 0, cnt, reinterpret_cast<const jshort *>(tbl));
    return arr;
}

jshort readRds(JNIEnv *env __unused, jobject thiz __unused)
{
    if (pFMRadio == nullptr)
        return 0;
    return static_cast<jshort>(pFMRadio->ReadRDS());
}

jbyteArray getPs(JNIEnv *env, jobject thiz __unused)
{
    if (pFMRadio == nullptr)
        return nullptr;
    ServiceName ps = pFMRadio->GetPs();
    if (ps.iLenght == 0)
        return nullptr;
    jbyteArray PSName = env->NewByteArray(ps.iLenght);
    env->SetByteArrayRegion(PSName, 0, ps.iLenght, reinterpret_cast<const jbyte *>(ps.Text));
    return PSName;
}

jbyteArray getLrText(JNIEnv *env, jobject thiz __unused)
{
    if (pFMRadio == nullptr)
        return nullptr;
    RadioText rt = pFMRadio->GetLrText();
    if (rt.iLenght == 0)
        return nullptr;
    jbyteArray SName = env->NewByteArray(rt.iLenght);
    env->SetByteArrayRegion(SName, 0, rt.iLenght, reinterpret_cast<const jbyte *>(rt.Text));
    return SName;
}

jshort activeAf(JNIEnv *env __unused, jobject thiz __unused)
{
    if (pFMRadio == nullptr)
        return 0;
    long ret = pFMRadio->GetChannel();
    return static_cast<jshort>(ret / 100); /* 875..1080 */
}

jshortArray getAFList(JNIEnv *env __unused, jobject thiz __unused)
{
    return nullptr;
}

jint setRds(JNIEnv *env __unused, jobject thiz __unused, jboolean rdson)
{
    if (pFMRadio == nullptr)
        return JNI_FALSE;
    if (rdson == JNI_TRUE)
        pFMRadio->EnableRDS();
    else
        pFMRadio->DisableRDS();
    return JNI_TRUE;
}

jboolean stopScan(JNIEnv *env __unused, jobject thiz __unused)
{
    if (pFMRadio != nullptr) {
        pFMRadio->StopScan();
        pFMRadio->SeekCancel();
    }
    return JNI_TRUE;
}

jint setMute(JNIEnv *env __unused, jobject thiz __unused, jboolean mute)
{
    if (pFMRadio == nullptr)
        return JNI_FALSE;
    if (mute == JNI_TRUE)
        pFMRadio->MuteOn();
    else
        pFMRadio->MuteOff();
    return JNI_TRUE;
}

jint isRdsSupport(JNIEnv *env __unused, jobject thiz __unused)
{
    return 1;
}

jint switchAntenna(JNIEnv *env __unused, jobject thiz __unused, jint antenna __unused)
{
    return 2; /* not supported */
}

static const char *classPathNameRx = "com/android/fmradio/FmNative";

static JNINativeMethod methodsRx[] = {
    { "openDev", "()Z", (void *)openDev },
    { "closeDev", "()Z", (void *)closeDev },
    { "powerUp", "(F)Z", (void *)powerUp },
    { "powerDown", "(I)Z", (void *)powerDown },
    { "tune", "(F)Z", (void *)tune },
    { "seek", "(FZ)F", (void *)seek },
    { "autoScan", "()[S", (void *)autoScan },
    { "stopScan", "()Z", (void *)stopScan },
    { "setRds", "(Z)I", (void *)setRds },
    { "readRds", "()S", (void *)readRds },
    { "getPs", "()[B", (void *)getPs },
    { "getLrText", "()[B", (void *)getLrText },
    { "activeAf", "()S", (void *)activeAf },
    { "setMute", "(Z)I", (void *)setMute },
    { "isRdsSupport", "()I", (void *)isRdsSupport },
    { "switchAntenna", "(I)I", (void *)switchAntenna },
};

static jint registerNativeMethods(JNIEnv *env, const char *className,
                                  JNINativeMethod *gMethods, int numMethods)
{
    jclass clazz = env->FindClass(className);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    if (clazz == nullptr) {
        ALOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        ALOGE("RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

static jint registerNatives(JNIEnv *env)
{
    return registerNativeMethods(env, classPathNameRx, methodsRx,
                                 sizeof(methodsRx) / sizeof(methodsRx[0]));
}

typedef union {
    JNIEnv *env;
    void *venv;
} UnionJNIEnvToVoid;

jint JNI_OnLoad(JavaVM *vm, void *reserved __unused)
{
    UnionJNIEnvToVoid uenv;
    uenv.venv = nullptr;
    jint result = -1;
    JNIEnv *env = nullptr;

    ALOGI("JNI_OnLoad Broadcom FM+RDS");

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed");
        goto fail;
    }
    env = uenv.env;

    if (registerNatives(env) != JNI_TRUE) {
        ALOGE("ERROR: registerNatives failed");
        goto fail;
    }

    result = JNI_VERSION_1_4;

fail:
    return result;
}

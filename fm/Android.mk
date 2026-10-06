#
# Copyright (C) 2026 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

ifeq ($(BOARD_HAVE_BCM_FM),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    FmRadioController_brcm.cpp \
    libfm_jni.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(JNI_H_INCLUDE)

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog

LOCAL_CFLAGS += -Wall -Werror -Wno-unused-parameter

LOCAL_MODULE := libfmjni
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := samsung
LOCAL_PROPRIETARY_MODULE := false

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_HAVE_BCM_FM

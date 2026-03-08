#
# Copyright (C) 2019 The LineageOS Project
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

DEVICE_PATH := device/samsung/j5y17lte

# audio type guard
TARGET_DEVICE_HAS_TFA_SEC_AUDIO_HAL := false
TARGET_DEVICE_HAS_SEC_AUDIO_HAL := false
TARGET_DEVICE_HAS_OSS_AUDIO_HAL := true

ifeq ($(TARGET_DEVICE_HAS_TFA_SEC_AUDIO_HAL),true)
TARGET_DEVICE_HAS_TFA_AMP := true
TARGET_DEVICE_HAS_PREBUILT_AUDIO_HAL := true
else ifeq ($(TARGET_DEVICE_HAS_OSS_AUDIO_HAL),true)
TARGET_DEVICE_HAS_TFA_AMP := true
endif

# TFA
TFA_MODEL := 9890

# gatekeeper type guard
TARGET_DEVICE_HAS_HW_GATEKEEPER_BIOMETRICS := true
TARGET_DEVICE_HAS_HW_GATEKEEPER_COMMON := false

# radio type guard
TARGET_DEVICE_HAS_SEC_RIL := true

# gnss type guard
TARGET_DEVICE_HAS_SEC_GNSS := true

# prebuilt slsi
TARGET_DEVICE_HAS_SAMSUNG_SLSI_EXYNOS7870 := false

# keymaster type guard 
# TODO: Fix keymaster driver telling its untrusted
TARGET_DEVICE_HAS_SEC_KEYMASTER := false

# gatekeeper type guard
TARGET_DEVICE_HAS_HW_GATEKEEPER := true

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml \
    frameworks/native/data/etc/android.hardware.nfc.ese.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.ese.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.hcef.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hcef.xml \
    frameworks/native/data/etc/android.hardware.nfc.uicc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.uicc.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.xml

# Custom mixer_paths
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/audio/mixer_gains.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_gains.xml \
    $(DEVICE_PATH)/configs/audio/mixer_paths.xml:$(TARGET_COPY_OUT_VENDOR)/etc/mixer_paths.xml

# Bootanimation
TARGET_SCREEN_HEIGHT := 1280
TARGET_SCREEN_WIDTH := 720

# Graphics
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xhdpi
PRODUCT_AAPT_PREBUILT_DPI := xhdpi hdpi

# Fingerprint
PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.3-service.j5y17lte

# Fingerprint Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml

# Bluetooth
PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0-service \
    android.hardware.bluetooth.audio@2.0-impl \
    audio.bluetooth.default \
    libbt-vendor

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/bluetooth/bt_vendor.conf:vendor/etc/bluetooth/bt_vendor.conf

# Display Device Config
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/display/576nits_config/display_id_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/displayconfig/display_id_0.xml

# NFC
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/nfc/libnfc-sec-vendor.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-sec-vendor.conf \
    $(LOCAL_PATH)/configs/nfc/libnfc-nci.conf:$(TARGET_COPY_OUT_VENDOR)/etc/libnfc-nci.conf \
    $(LOCAL_PATH)/configs/nfc/nfcee_access.xml:$(TARGET_COPY_OUT_VENDOR)/etc/nfcee_access.xml

# NFC
PRODUCT_PACKAGES += \
    libnfc-nci \
    libnfc_nci_jni \
    NfcNci \
    Tag \
    com.android.nfc_extras \
    android.hardware.nfc@1.2.vendor \
    android.hardware.nfc@1.2-service.samsung

# Overlays
DEVICE_PACKAGE_OVERLAYS += \
    $(DEVICE_PATH)/overlay

# Ramdisk
PRODUCT_PACKAGES += \
    mobicore.rc \
    wifi_device.rc

# Shims
PRODUCT_PACKAGES += \
    libbauthtzcommon_shim

# Wifi
PRODUCT_PACKAGES += \
    android.hardware.wifi@1.0-service \
    hostapd \
    libwifi-hal \
    libwpa_client \
    wificond \
    wpa_supplicant \
    wpa_supplicant.conf

#hostpad
PRODUCT_PACKAGES += \
    android.hardware.wifi.hostapd@1.0

#wifi
PRODUCT_PACKAGES += \
    android.hardware.wifi@1.4

#supplicant
PRODUCT_PACKAGES += \
    android.hardware.wifi.supplicant@1.3

PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/wifi/p2p_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/p2p_supplicant_overlay.conf \
    $(DEVICE_PATH)/configs/wifi/wpa_supplicant_overlay.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant_overlay.conf

# Properties
-include $(DEVICE_PATH)/vendor_prop.mk

# Inherit from common
$(call inherit-product, device/samsung/universal7870-common/device-common.mk)

$(call inherit-product-if-exists, vendor/samsung/j5y17lte/j5y17lte-vendor.mk)

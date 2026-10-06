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
 *
 * Broadcom BCM43xx FM receiver (BCM43438/BCM43455) is the FM core sitting
 * on the combo chip. Registers match the public BCM2048 FM/RDS map and are
 * reached with vendor HCI 0xFC15 while Bluetooth is up. RDS Program Service
 * and RadioText are decoded here for libfmjni / packages/apps/FMRadio.
 */

#define LOG_TAG "FMLIB_BRCM"

#include "FmRadioController_brcm.h"

#include <cutils/properties.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <log/log.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/types.h>

namespace {

/* BCM2048-compatible FM register map (public silicon documentation). */
constexpr uint8_t kRegSystem     = 0x00;
constexpr uint8_t kRegFmCtrl     = 0x01;
constexpr uint8_t kRegRdsCtrl0   = 0x02;
constexpr uint8_t kRegAudioCtrl0 = 0x05;
constexpr uint8_t kRegSearchCtrl0 = 0x07;
constexpr uint8_t kRegTuneMode   = 0x09;
constexpr uint8_t kRegFreq0      = 0x0a;
constexpr uint8_t kRegFreq1      = 0x0b;
constexpr uint8_t kRegRssi       = 0x0f;
constexpr uint8_t kRegRdsFlag0   = 0x12;
constexpr uint8_t kRegRdsWline   = 0x14;
constexpr uint8_t kRegRdsData    = 0x80;

constexpr uint8_t kFmOn  = 0x01;
constexpr uint8_t kRdsOn = 0x02;

constexpr uint8_t kStereoAuto = 0x02;

constexpr uint8_t kRfMute     = 0x01;
constexpr uint8_t kManualMute = 0x02;
constexpr uint8_t kDacLeft    = 0x04;
constexpr uint8_t kDacRight   = 0x08;
constexpr uint8_t kRouteDac   = 0x10;
constexpr uint8_t kRouteI2s   = 0x20;

constexpr uint8_t kSearchDirUp = 0x80;
constexpr uint8_t kTunePreset  = 0x01;
constexpr uint8_t kTuneSearch  = 0x02;
constexpr uint8_t kFlagTuneOk  = 0x01;
constexpr uint8_t kFlagTuneFail = 0x02;
constexpr uint8_t kRdsFifoWline = 0x02;

constexpr uint32_t kFreqBaseKhz = 64000;
constexpr long kBandLowKhz  = 87500;
constexpr long kBandHighKhz = 108000;
constexpr long kSpacingKhz  = 100;
constexpr uint8_t kRssiSeekTh = 0x64;

constexpr uint16_t kHciFmOpcode = 0xFC15; /* OGF 0x3F, OCF 0x15 */

/* Minimal HCI socket bits so we do not depend on libbluetooth. */
constexpr int kPfBluetooth = 31;
constexpr int kBtprotoHci  = 1;
constexpr int kSolHci      = 0;
constexpr int kHciFilter   = 2;
constexpr unsigned short kHciChannelRaw = 0;

struct sockaddr_hci {
    sa_family_t hci_family;
    unsigned short hci_dev;
    unsigned short hci_channel;
};

struct hci_filter {
    uint32_t type_mask;
    uint32_t event_mask[2];
    uint16_t opcode;
};

void hciFilterSet(struct hci_filter *f, uint16_t opcode) {
    memset(f, 0, sizeof(*f));
    f->type_mask = 1u << 0x04; /* HCI event */
    /* event 0x0e Command Complete */
    f->event_mask[0] = 1u << 0x0e;
    f->opcode = opcode;
}

int writeFile(const char *path, const char *value) {
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -errno;
    ssize_t n = write(fd, value, strlen(value));
    close(fd);
    return n < 0 ? -errno : 0;
}

int findLnaPath(char *out, size_t out_len) {
    const char *candidates[] = {
        "/sys/devices/platform/bluetooth/lna_en",
        "/sys/devices/platform/bcm43xx_bluetooth/lna_en",
        nullptr,
    };
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], W_OK) == 0) {
            snprintf(out, out_len, "%s", candidates[i]);
            return 0;
        }
    }

    /* Walk platform devices for a lna_en attribute created by bcm43xx. */
    DIR *dir = opendir("/sys/devices/platform");
    if (!dir)
        return -ENOENT;
    struct dirent *de;
    while ((de = readdir(dir)) != nullptr) {
        if (de->d_name[0] == '.')
            continue;
        char path[256];
        snprintf(path, sizeof(path), "/sys/devices/platform/%s/lna_en", de->d_name);
        if (access(path, W_OK) == 0) {
            snprintf(out, out_len, "%s", path);
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return -ENOENT;
}

}  // namespace

FmRadioController_brcm::FmRadioController_brcm()
    : hci_fd(-1),
      powered(0),
      rds_on(0),
      mute_on(0),
      scan_stop(0),
      current_khz(100000),
      pending_rds_events(0),
      ps_mask(0),
      rt_ab(0xff),
      rt_mask(0),
      rt_len(0),
      rds_thread(0),
      rds_thread_run(0) {
    pthread_mutex_init(&lock, nullptr);
    memset(&ps, 0, sizeof(ps));
    memset(&rt, 0, sizeof(rt));
    memset(ps_buf, 0, sizeof(ps_buf));
    memset(rt_buf, 0, sizeof(rt_buf));
}

FmRadioController_brcm::~FmRadioController_brcm() {
    DisableRDS();
    powerOff();
    setLna(0);
    closeHci();
    pthread_mutex_destroy(&lock);
}

int FmRadioController_brcm::openHci() {
    if (hci_fd >= 0)
        return 0;

    int dev = property_get_int32("ro.vendor.fm.hci_dev", 0);
    int fd = socket(kPfBluetooth, SOCK_RAW | SOCK_CLOEXEC, kBtprotoHci);
    if (fd < 0) {
        ALOGE("HCI socket failed: %s", strerror(errno));
        return FM_FAILURE;
    }

    struct sockaddr_hci addr;
    memset(&addr, 0, sizeof(addr));
    addr.hci_family = kPfBluetooth;
    addr.hci_dev = static_cast<unsigned short>(dev);
    addr.hci_channel = kHciChannelRaw;
    if (bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        ALOGE("HCI bind hci%d failed: %s", dev, strerror(errno));
        close(fd);
        return FM_FAILURE;
    }

    struct hci_filter flt;
    hciFilterSet(&flt, kHciFmOpcode);
    setsockopt(fd, kSolHci, kHciFilter, &flt, sizeof(flt));

    hci_fd = fd;
    ALOGI("opened HCI hci%d for Broadcom FM", dev);
    return 0;
}

void FmRadioController_brcm::closeHci() {
    if (hci_fd >= 0) {
        close(hci_fd);
        hci_fd = -1;
    }
}

int FmRadioController_brcm::hciCommand(uint16_t opcode, const uint8_t *param,
                                       uint8_t plen, uint8_t *resp,
                                       size_t resp_max, int *resp_len) {
    if (hci_fd < 0)
        return FM_FAILURE;

    uint8_t cmd[4 + 255];
    cmd[0] = 0x01; /* HCI command packet */
    cmd[1] = static_cast<uint8_t>(opcode & 0xff);
    cmd[2] = static_cast<uint8_t>(opcode >> 8);
    cmd[3] = plen;
    if (plen && param)
        memcpy(cmd + 4, param, plen);

    if (write(hci_fd, cmd, 4u + plen) < 0) {
        ALOGE("HCI write 0x%04x failed: %s", opcode, strerror(errno));
        return FM_FAILURE;
    }

    struct pollfd pfd = { .fd = hci_fd, .events = POLLIN, .revents = 0 };
    int pret = poll(&pfd, 1, 1000);
    if (pret <= 0) {
        ALOGE("HCI 0x%04x timed out", opcode);
        return FM_FAILURE;
    }

    uint8_t buf[260];
    ssize_t n = read(hci_fd, buf, sizeof(buf));
    if (n < 6 || buf[0] != 0x04 || buf[1] != 0x0e) {
        ALOGE("HCI 0x%04x unexpected event (n=%zd)", opcode, n);
        return FM_FAILURE;
    }
    /* EVT: 04 0e plen ncmd opcode_lo opcode_hi status [params] */
    uint8_t status = buf[6];
    if (status != 0) {
        ALOGE("HCI 0x%04x status 0x%02x", opcode, status);
        return FM_FAILURE;
    }
    if (resp && resp_len) {
        int extra = static_cast<int>(n) - 7;
        if (extra < 0)
            extra = 0;
        if (extra > static_cast<int>(resp_max))
            extra = static_cast<int>(resp_max);
        if (extra > 0)
            memcpy(resp, buf + 7, extra);
        *resp_len = extra;
    }
    return FM_SUCCESS;
}

int FmRadioController_brcm::fmWrite(uint8_t reg, uint8_t value) {
    uint8_t p[3] = { reg, 0x00, value };
    return hciCommand(kHciFmOpcode, p, 3, nullptr, 0, nullptr);
}

int FmRadioController_brcm::fmRead(uint8_t reg, uint8_t *value) {
    uint8_t p[2] = { reg, 0x00 };
    uint8_t resp[16];
    int rlen = 0;
    if (hciCommand(kHciFmOpcode, p, 2, resp, sizeof(resp), &rlen) != FM_SUCCESS)
        return FM_FAILURE;
    if (rlen < 1) {
        ALOGE("FM read reg 0x%02x empty response", reg);
        return FM_FAILURE;
    }
    *value = resp[rlen - 1];
    return FM_SUCCESS;
}

int FmRadioController_brcm::setLna(int enable) {
    char path[256];
    if (findLnaPath(path, sizeof(path)) != 0) {
        ALOGW("FM LNA sysfs node not found");
        return FM_FAILURE;
    }
    int ret = writeFile(path, enable ? "1\n" : "0\n");
    ALOGI("FM LNA %s (%s) ret=%d", enable ? "on" : "off", path, ret);
    return ret == 0 ? FM_SUCCESS : FM_FAILURE;
}

int FmRadioController_brcm::powerOn() {
    if (powered)
        return FM_SUCCESS;

    if (openHci() != 0)
        return FM_FAILURE;

    setLna(1);
    usleep(20000);

    if (fmWrite(kRegSystem, kFmOn) != FM_SUCCESS)
        return FM_FAILURE;
    usleep(20000);

    /* Europe/US band, auto stereo blend. */
    fmWrite(kRegFmCtrl, kStereoAuto);

    /* Unmute, DAC+I2S route so analog/I2S audio reaches the codec fm_dummy. */
    uint8_t aud = kDacLeft | kDacRight | kRouteDac | kRouteI2s;
    fmWrite(kRegAudioCtrl0, aud);

    fmWrite(kRegSearchCtrl0, kRssiSeekTh);
    powered = 1;
    mute_on = 0;
    ALOGI("Broadcom FM powered on");
    return FM_SUCCESS;
}

int FmRadioController_brcm::powerOff() {
    if (!powered)
        return FM_SUCCESS;
    fmWrite(kRegSystem, 0x00);
    setLna(0);
    powered = 0;
    ALOGI("Broadcom FM powered off");
    return FM_SUCCESS;
}

int FmRadioController_brcm::setFrequencyKhz(long khz) {
    if (khz < kBandLowKhz)
        khz = kBandLowKhz;
    if (khz > kBandHighKhz)
        khz = kBandHighKhz;

    uint16_t raw = static_cast<uint16_t>(khz - kFreqBaseKhz);
    if (fmWrite(kRegFreq0, static_cast<uint8_t>(raw & 0xff)) != FM_SUCCESS)
        return FM_FAILURE;
    if (fmWrite(kRegFreq1, static_cast<uint8_t>(raw >> 8)) != FM_SUCCESS)
        return FM_FAILURE;
    if (fmWrite(kRegTuneMode, kTunePreset) != FM_SUCCESS)
        return FM_FAILURE;
    waitTuneComplete(1500);
    current_khz = khz;
    return FM_SUCCESS;
}

long FmRadioController_brcm::getFrequencyKhz() {
    uint8_t lo = 0, hi = 0;
    if (fmRead(kRegFreq0, &lo) != FM_SUCCESS || fmRead(kRegFreq1, &hi) != FM_SUCCESS)
        return current_khz;
    long khz = (static_cast<long>(hi) << 8 | lo) + kFreqBaseKhz;
    if (khz >= kBandLowKhz && khz <= kBandHighKhz)
        current_khz = khz;
    return current_khz;
}

int FmRadioController_brcm::waitTuneComplete(int timeout_ms) {
    const int step = 20;
    for (int t = 0; t < timeout_ms; t += step) {
        uint8_t flags = 0;
        if (fmRead(kRegRdsFlag0, &flags) == FM_SUCCESS) {
            if (flags & kFlagTuneOk)
                return FM_SUCCESS;
            if (flags & kFlagTuneFail)
                return FM_FAILURE;
        }
        usleep(step * 1000);
    }
    return FM_SUCCESS; /* some firmware never raises the flag */
}

int FmRadioController_brcm::seekInternal(int upward) {
    uint8_t ctrl = kRssiSeekTh;
    if (upward)
        ctrl |= kSearchDirUp;
    fmWrite(kRegSearchCtrl0, ctrl);
    fmWrite(kRegTuneMode, kTuneSearch);
    if (waitTuneComplete(3000) != FM_SUCCESS)
        return FM_FAILURE;
    return FM_SUCCESS;
}

int FmRadioController_brcm::Initialise() {
    pthread_mutex_lock(&lock);
    int ret = powerOn();
    pthread_mutex_unlock(&lock);
    return ret;
}

void FmRadioController_brcm::TuneChannel(long channel_khz) {
    pthread_mutex_lock(&lock);
    if (!powered && powerOn() != FM_SUCCESS) {
        pthread_mutex_unlock(&lock);
        return;
    }
    setFrequencyKhz(channel_khz);
    /* Fresh station: drop stale RDS. */
    pending_rds_events = 0;
    ps_mask = 0;
    rt_mask = 0;
    rt_len = 0;
    memset(ps_buf, 0, sizeof(ps_buf));
    memset(rt_buf, 0, sizeof(rt_buf));
    memset(&ps, 0, sizeof(ps));
    memset(&rt, 0, sizeof(rt));
    pthread_mutex_unlock(&lock);
}

long FmRadioController_brcm::GetChannel() {
    pthread_mutex_lock(&lock);
    long khz = powered ? getFrequencyKhz() : current_khz;
    pthread_mutex_unlock(&lock);
    return khz;
}

long FmRadioController_brcm::SeekUp() {
    pthread_mutex_lock(&lock);
    if (!powered && powerOn() != FM_SUCCESS) {
        pthread_mutex_unlock(&lock);
        return current_khz;
    }
    seekInternal(1);
    long khz = getFrequencyKhz();
    pthread_mutex_unlock(&lock);
    return khz;
}

long FmRadioController_brcm::SeekDown() {
    pthread_mutex_lock(&lock);
    if (!powered && powerOn() != FM_SUCCESS) {
        pthread_mutex_unlock(&lock);
        return current_khz;
    }
    seekInternal(0);
    long khz = getFrequencyKhz();
    pthread_mutex_unlock(&lock);
    return khz;
}

void FmRadioController_brcm::SeekCancel() {
    scan_stop = 1;
    pthread_mutex_lock(&lock);
    if (powered)
        fmWrite(kRegTuneMode, 0x00);
    pthread_mutex_unlock(&lock);
}

void FmRadioController_brcm::MuteOn() {
    pthread_mutex_lock(&lock);
    uint8_t aud = kManualMute | kRfMute | kDacLeft | kDacRight | kRouteDac | kRouteI2s;
    fmWrite(kRegAudioCtrl0, aud);
    mute_on = 1;
    pthread_mutex_unlock(&lock);
}

void FmRadioController_brcm::MuteOff() {
    pthread_mutex_lock(&lock);
    uint8_t aud = kDacLeft | kDacRight | kRouteDac | kRouteI2s;
    fmWrite(kRegAudioCtrl0, aud);
    mute_on = 0;
    pthread_mutex_unlock(&lock);
}

void *FmRadioController_brcm::rdsThread(void *arg) {
    auto *self = static_cast<FmRadioController_brcm *>(arg);
    while (self->rds_thread_run) {
        pthread_mutex_lock(&self->lock);
        if (self->powered && self->rds_on)
            self->parseRdsFifo();
        pthread_mutex_unlock(&self->lock);
        usleep(100000);
    }
    return nullptr;
}

void FmRadioController_brcm::EnableRDS() {
    pthread_mutex_lock(&lock);
    if (!powered)
        powerOn();
    uint8_t sys = kFmOn | kRdsOn;
    fmWrite(kRegSystem, sys);
    fmWrite(kRegRdsCtrl0, 0x02); /* flush FIFO */
    fmWrite(kRegRdsWline, 0x10);
    rds_on = 1;
    if (!rds_thread_run) {
        rds_thread_run = 1;
        pthread_create(&rds_thread, nullptr, rdsThread, this);
    }
    pthread_mutex_unlock(&lock);
    ALOGI("RDS enabled");
}

void FmRadioController_brcm::DisableRDS() {
    pthread_mutex_lock(&lock);
    rds_on = 0;
    if (powered)
        fmWrite(kRegSystem, kFmOn);
    pthread_mutex_unlock(&lock);
    if (rds_thread_run) {
        rds_thread_run = 0;
        pthread_join(rds_thread, nullptr);
        rds_thread = 0;
    }
}

void FmRadioController_brcm::parseRdsFifo() {
    uint8_t flags = 0;
    if (fmRead(kRegRdsFlag0, &flags) != FM_SUCCESS)
        return;
    if (!(flags & kRdsFifoWline))
        return;

    /* Each FIFO entry is a 3-byte duple: data LSB, data MSB, block/CRC flags. */
    uint16_t blocks[4] = {0, 0, 0, 0};
    int have = 0;
    for (int i = 0; i < 48; i++) {
        uint8_t b0 = 0, b1 = 0, b2 = 0;
        if (fmRead(kRegRdsData, &b0) != FM_SUCCESS)
            break;
        if (fmRead(kRegRdsData, &b1) != FM_SUCCESS)
            break;
        if (fmRead(kRegRdsData, &b2) != FM_SUCCESS)
            break;

        if (b0 == 0x7c && b1 == 0xff)
            break; /* end marker used by BCM2048 */

        int blk = (b2 & 0xf0);
        uint16_t data = static_cast<uint16_t>(b1) << 8 | b0;
        int crc = b2 & 0x0c;
        if (crc == 0x0c)
            continue; /* unrecoverable */

        int idx = -1;
        if (blk == 0x00)
            idx = 0;
        else if (blk == 0x10)
            idx = 1;
        else if (blk == 0x20 || blk == 0x40)
            idx = 2;
        else if (blk == 0x30)
            idx = 3;
        if (idx < 0)
            continue;
        blocks[idx] = data;
        have |= 1 << idx;
        if (have == 0x0f) {
            handleRdsGroup(blocks[0], blocks[1], blocks[2], blocks[3]);
            have = 0;
        }
    }
}

void FmRadioController_brcm::handleRdsGroup(uint16_t b0, uint16_t b1,
                                            uint16_t b2, uint16_t b3) {
    int gtype = (b1 >> 12) & 0x0f;
    int gver  = (b1 >> 11) & 0x01; /* 0 = A, 1 = B */

    if (gtype == 0) {
        int idx = b1 & 0x03;
        char c0 = static_cast<char>((b3 >> 8) & 0xff);
        char c1 = static_cast<char>(b3 & 0xff);
        if (idx >= 0 && idx < 4) {
            ps_buf[idx * 2]     = c0;
            ps_buf[idx * 2 + 1] = c1;
            ps_mask |= static_cast<uint8_t>(1u << idx);
        }
        if (ps_mask == 0x0f) {
            memcpy(ps.Text, ps_buf, PS_MAXIMUM_SIZE);
            ps.Text[PS_MAXIMUM_SIZE] = 0;
            ps.iLenght = PS_MAXIMUM_SIZE;
            pending_rds_events |= RDS_EVT_PS_UPDATE;
            ps_mask = 0;
        }
        (void)gver;
        (void)b0;
        (void)b2;
        return;
    }

    if (gtype == 2) {
        int ab = (b1 >> 4) & 0x01;
        int idx = b1 & 0x0f;
        if (ab != rt_ab) {
            memset(rt_buf, 0, sizeof(rt_buf));
            rt_mask = 0;
            rt_ab = static_cast<uint8_t>(ab);
        }
        if (gver == 0) {
            /* 2A: 4 characters from C+D */
            int pos = idx * 4;
            if (pos + 3 < RT_MAXIMUM_SIZE) {
                rt_buf[pos]     = static_cast<char>((b2 >> 8) & 0xff);
                rt_buf[pos + 1] = static_cast<char>(b2 & 0xff);
                rt_buf[pos + 2] = static_cast<char>((b3 >> 8) & 0xff);
                rt_buf[pos + 3] = static_cast<char>(b3 & 0xff);
                rt_mask |= 1u << idx;
                if (rt_len < pos + 4)
                    rt_len = pos + 4;
            }
        } else {
            /* 2B: 2 characters from D */
            int pos = idx * 2;
            if (pos + 1 < RT_MAXIMUM_SIZE) {
                rt_buf[pos]     = static_cast<char>((b3 >> 8) & 0xff);
                rt_buf[pos + 1] = static_cast<char>(b3 & 0xff);
                rt_mask |= 1u << idx;
                if (rt_len < pos + 2)
                    rt_len = pos + 2;
            }
        }
        if (rt_len > 0) {
            memcpy(rt.Text, rt_buf, RT_MAXIMUM_SIZE);
            rt.Text[RT_MAXIMUM_SIZE] = 0;
            rt.iLenght = static_cast<unsigned char>(rt_len > RT_MAXIMUM_SIZE ? RT_MAXIMUM_SIZE : rt_len);
            pending_rds_events |= RDS_EVT_RT_UPDATE;
        }
    }
}

ServiceName FmRadioController_brcm::GetPs() {
    pthread_mutex_lock(&lock);
    ServiceName out = ps;
    pthread_mutex_unlock(&lock);
    return out;
}

RadioText FmRadioController_brcm::GetLrText() {
    pthread_mutex_lock(&lock);
    RadioText out = rt;
    pthread_mutex_unlock(&lock);
    return out;
}

int FmRadioController_brcm::ReadRDS() {
    pthread_mutex_lock(&lock);
    int ev = pending_rds_events;
    pending_rds_events = 0;
    pthread_mutex_unlock(&lock);
    return ev;
}

int FmRadioController_brcm::AutoScan(uint16_t *out, int max_count) {
    int n = 0;
    scan_stop = 0;
    pthread_mutex_lock(&lock);
    if (!powered && powerOn() != FM_SUCCESS) {
        pthread_mutex_unlock(&lock);
        return 0;
    }
    pthread_mutex_unlock(&lock);

    for (long khz = kBandLowKhz; khz <= kBandHighKhz && n < max_count; khz += kSpacingKhz) {
        if (scan_stop)
            break;
        pthread_mutex_lock(&lock);
        setFrequencyKhz(khz);
        uint8_t rssi = 0;
        fmRead(kRegRssi, &rssi);
        pthread_mutex_unlock(&lock);
        if (rssi >= kRssiSeekTh) {
            out[n++] = static_cast<uint16_t>(khz / 100); /* 875..1080 */
        }
    }
    return n;
}

void FmRadioController_brcm::StopScan() {
    scan_stop = 1;
}

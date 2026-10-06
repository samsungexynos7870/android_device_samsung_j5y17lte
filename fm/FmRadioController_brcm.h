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

#ifndef FMRADIO_CONTROLLER_BRCM_H
#define FMRADIO_CONTROLLER_BRCM_H

#include <cstddef>
#include <pthread.h>
#include <stdint.h>

#define FM_FAILURE (-1)
#define FM_SUCCESS 0

#define PS_MAXIMUM_SIZE 8
#define RT_MAXIMUM_SIZE 64

/* RDS event bits expected by packages/apps/FMRadio FmService */
#define RDS_EVT_PS_UPDATE 0x0008
#define RDS_EVT_RT_UPDATE 0x0040
#define RDS_EVT_AF        0x0080

typedef struct {
    char Text[PS_MAXIMUM_SIZE + 1];
    unsigned char iLenght;
} ServiceName;

typedef struct {
    char Text[RT_MAXIMUM_SIZE + 1];
    unsigned char iLenght;
} RadioText;

class FmRadioController_brcm {
public:
    FmRadioController_brcm();
    ~FmRadioController_brcm();

    int Initialise();
    void TuneChannel(long channel_khz);
    long GetChannel();
    long SeekUp();
    long SeekDown();
    void SeekCancel();

    void MuteOn();
    void MuteOff();

    void EnableRDS();
    void DisableRDS();
    ServiceName GetPs();
    RadioText GetLrText();
    int ReadRDS();

    int AutoScan(uint16_t *out, int max_count);
    void StopScan();

private:
    int hci_fd;
    int powered;
    int rds_on;
    int mute_on;
    int scan_stop;
    long current_khz;
    uint16_t pending_rds_events;

    ServiceName ps;
    RadioText rt;

    char ps_buf[PS_MAXIMUM_SIZE];
    uint8_t ps_mask;
    char rt_buf[RT_MAXIMUM_SIZE];
    uint8_t rt_ab;
    uint32_t rt_mask;
    int rt_len;

    pthread_mutex_t lock;
    pthread_t rds_thread;
    int rds_thread_run;

    int openHci();
    void closeHci();
    int hciCommand(uint16_t opcode, const uint8_t *param, uint8_t plen,
                   uint8_t *resp, size_t resp_max, int *resp_len);
    int fmWrite(uint8_t reg, uint8_t value);
    int fmRead(uint8_t reg, uint8_t *value);

    int setLna(int enable);
    int powerOn();
    int powerOff();
    int setFrequencyKhz(long khz);
    long getFrequencyKhz();
    int waitTuneComplete(int timeout_ms);
    int seekInternal(int upward);
    void parseRdsFifo();
    void handleRdsGroup(uint16_t b0, uint16_t b1, uint16_t b2, uint16_t b3);
    static void *rdsThread(void *arg);
};

#endif

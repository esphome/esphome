#pragma once

namespace esphome {
namespace gdew0154m09 {
static const int CLEAR_MODE_FULL = 0;
static const int CLEAR_MODE_PARTIAL = 1;

static const int CMD_DTM1_DATA_START_TRANS = 0x10;
static const int CMD_DTM2_DATA_START_TRANS2 = 0x13;
static const int CMD_DISPLAY_REFRESH = 0x12;
static const int CMD_PSR_PANEL_SETTING = 0x00;
static const int CMD_UNDOCUMENTED_0x4D = 0x4D;
static const int CMD_UNDOCUMENTED_0xAA = 0xaa;
static const int CMD_UNDOCUMENTED_0xE9 = 0xe9;
static const int CMD_UNDOCUMENTED_0xB6 = 0xb6;
static const int CMD_UNDOCUMENTED_0xF3 = 0xf3;
static const int CMD_TRES_RESOLUTION_SETTING = 0x61;
static const int CMD_TCON_TCONSETTING = 0x60;
static const int CMD_CDI_VCOM_DATA_INTERVAL = 0x50;
static const int CMD_POF_POWER_OFF = 0x02;
static const int CMD_DSLP_DEEP_SLEEP = 0x07;
static const int DATA_DSLP_DEEP_SLEEP = 0xA5;
static const int CMD_PWS_POWER_SAVING = 0xe3;
static const int CMD_PON_POWER_ON = 0x04;
static const int CMD_PTL_PARTIAL_WINDOW = 0x90;
}  // namespace gdew0154m09
}  // namespace esphome

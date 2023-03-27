#pragma once

namespace esphome {
namespace gdew0154m09 {
static const uint8_t CLEAR_MODE_FULL = 0;
static const uint8_t CLEAR_MODE_PARTIAL = 1;

static const uint8_t CMD_DTM1_DATA_START_TRANS = 0x10;
static const uint8_t CMD_DTM2_DATA_START_TRANS2 = 0x13;
static const uint8_t CMD_DISPLAY_REFRESH = 0x12;
static const uint8_t CMD_PSR_PANEL_SETTING = 0x00;
static const uint8_t CMD_UNDOCUMENTED_0x4D = 0x4D;  //  NOLINT
static const uint8_t CMD_UNDOCUMENTED_0xAA = 0xaa;  //  NOLINT
static const uint8_t CMD_UNDOCUMENTED_0xE9 = 0xe9;  //  NOLINT
static const uint8_t CMD_UNDOCUMENTED_0xB6 = 0xb6;  //  NOLINT
static const uint8_t CMD_UNDOCUMENTED_0xF3 = 0xf3;  //  NOLINT
static const uint8_t CMD_TRES_RESOLUTION_SETTING = 0x61;
static const uint8_t CMD_TCON_TCONSETTING = 0x60;
static const uint8_t CMD_CDI_VCOM_DATA_INTERVAL = 0x50;
static const uint8_t CMD_POF_POWER_OFF = 0x02;
static const uint8_t CMD_DSLP_DEEP_SLEEP = 0x07;
static const uint8_t DATA_DSLP_DEEP_SLEEP = 0xA5;
static const uint8_t CMD_PWS_POWER_SAVING = 0xe3;
static const uint8_t CMD_PON_POWER_ON = 0x04;
static const uint8_t CMD_PTL_PARTIAL_WINDOW = 0x90;
}  // namespace gdew0154m09
}  // namespace esphome

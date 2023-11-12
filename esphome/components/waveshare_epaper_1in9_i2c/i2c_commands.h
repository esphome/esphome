#pragma once

#include "esphome/core/hal.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static const uint8_t CMD_POWER_OFF = 0x28;
static const uint8_t CMD_POWER_ON = 0x2B;

static const uint8_t CMD_RAM_ADDR_0 = 0x40;

static const uint8_t CMD_WAVEFORM_SET = 0x82;

static const uint8_t CMD_DCDC_BOOST_X8 = 0xA7;

static const uint8_t CMD_DATA_1_LATCH_OFF = 0xA8;
static const uint8_t CMD_DATA_1_LATCH_ON = 0xA9;

static const uint8_t CMD_DATA_2_LATCH_OFF = 0xAA;
static const uint8_t CMD_DATA_2_LATCH_ON = 0xAB;

static const uint8_t CMD_SLEEP_OFF = 0xAC;
static const uint8_t CMD_SLEEP_ON = 0xAD;

static const uint8_t CMD_DISPLAY_OFF = 0xAE;
static const uint8_t CMD_DISPLAY_ON = 0xAF;

}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome

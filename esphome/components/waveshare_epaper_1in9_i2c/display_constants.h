#pragma once

#include "esphome/core/hal.h"

#include "i2c_commands.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static const int16_t TEMPERATURE_X10_MIN = -99;
static const int16_t TEMPERATURE_X10_MAX = 1999;

static const int16_t HUMIDITY_X10_MIN = -99;
static const int16_t HUMIDITY_X10_MAX = 999;

static const unsigned LUT_SIZE = 7;

// 5S waveform for better anti-ghosting
static const uint8_t LUT_5S[LUT_SIZE] = {CMD_WAVEFORM_SET, 0x28, 0x20, 0xA8, 0xA0, 0x50, 0x65};

// White extinction diagram + black out diagram
static const uint8_t LUT_DU_WB[LUT_SIZE] = {CMD_WAVEFORM_SET, 0x80, 0x00, 0xC0, 0x80, 0x80, 0x62};

static const unsigned FRAMEBUFFER_SIZE = 15;

static const uint8_t CHAR_EMPTY = 0x00;
static const uint8_t CHAR_CELSIUS = 0x05;
static const uint8_t CHAR_FAHRENHEIT = 0x06;

static const unsigned CHAR_SLOTS = 2;
static const uint8_t CHAR_MINUS_SIGN[CHAR_SLOTS] = {0b01000100, 0b00000};
static const uint8_t CHAR_DIGITS[10][CHAR_SLOTS] = {
    {0xbf, 0xff},  // 0
    {0x00, 0xff},  // 1
    {0xfd, 0x17},  // 2
    {0xf5, 0xff},  // 3
    {0x47, 0xff},  // 4
    {0xf7, 0x1d},  // 5
    {0xff, 0x1d},  // 6
    {0x21, 0xff},  // 7
    {0xff, 0xff},  // 8
    {0xf7, 0xff},  // 9
};

static const unsigned TEMPERATURE_DIGITS_LEN = 4;
static const unsigned HUMIDITY_DIGITS_LEN = 3;

static const unsigned TEMPERATURE_DOT_INDEX = 4;
static const unsigned HUMIDITY_DOT_IDX = 8;
static const unsigned HUMIDITY_PERCENTAGE_IDX = 10;
static const unsigned INDICATORS_IDX = 13;  // Framebuffer position for °C/F/low power/BT indicators

// Bitmask to show the decimal dot
static const unsigned DOT_MASK = 0b0000000000100000;

// Bitmask to show humidity % sign
static const unsigned PERCENT_MASK = 0b0000000000100000;

// Bitmask to enable the low power indicator (empty battery symbol)
static const unsigned LOW_POWER_ON_MASK = 0b0000000000010000;

// Bitmask to enable the BT indicator
static const unsigned BT_ON_MASK = 0b0000000000001000;

}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome

#pragma once

#include <cinttypes>

namespace esphome {
namespace st7567_base {

//
// commands for ST7567. "base"
//
static const uint8_t ST7567_BOOSTER_ON = 0x2C;    // internal power supply on
static const uint8_t ST7567_REGULATOR_ON = 0x2E;  // internal power supply on
static const uint8_t ST7567_POWER_ON = 0x2F;      // internal power supply on

static const uint8_t ST7567_DISPLAY_ON = 0xAF;   // Display ON. Normal Display Mode.
static const uint8_t ST7567_DISPLAY_OFF = 0xAE;  // Display OFF. All SEGs/COMs output with VSS
static const uint8_t ST7567_SET_START_LINE = 0x40;
static const uint8_t ST7567_POWER_CTL = 0x28;
static const uint8_t ST7567_SEG_NORMAL = 0xA0;       //
static const uint8_t ST7567_SEG_REVERSE = 0xA1;      // mirror X axis (horizontal)
static const uint8_t ST7567_COM_NORMAL = 0xC0;       //
static const uint8_t ST7567_COM_REMAP = 0xC8;        // mirror Y axis (vertical)
static const uint8_t ST7567_PIXELS_NORMAL = 0xA4;    // display ram content
static const uint8_t ST7567_PIXELS_ALL_ON = 0xA5;    // all pixels on
static const uint8_t ST7567_INVERT_OFF = 0xA6;       // normal pixels
static const uint8_t ST7567_INVERT_ON = 0xA7;        // inverted pixels
static const uint8_t ST7567_SCAN_START_LINE = 0x40;  // scrolling = 0x40 + (0..63)
static const uint8_t ST7567_COL_ADDR_H = 0x10;       // x pos (0..95) 4 MSB
static const uint8_t ST7567_COL_ADDR_L = 0x00;       // x pos (0..95) 4 LSB
static const uint8_t ST7567_PAGE_ADDR = 0xB0;        // y pos, 8.5 rows (0..8)
static const uint8_t ST7567_BIAS_9 = 0xA2;           // 1/9 bias
static const uint8_t ST7567_CONTRAST = 0x80;         // 0x80 + (0..31)
static const uint8_t ST7567_SET_EV_CMD = 0x81;
static const uint8_t ST7567_SET_EV_PARAM = 0x00;
static const uint8_t ST7567_RESISTOR_RATIO = 0x20;
static const uint8_t ST7567_SW_REFRESH = 0xE2;

static const uint8_t ST7567_CONTRAST_MASK = 0b111111;
static const uint8_t ST7567_BRIGHTNESS_MASK = 0b111;
//
// commands for ST7570, which differ from ST7567 "base"
//
static const uint8_t ST7570_OSCILLATOR_ON = 0b10101011;
static const uint8_t ST7570_MODE_SET = 0b00111000;

// for future support:
// static const uint8_t ST7591_SET_START_LINE = 0b11010000;
// static const uint8_t ST7591_PAGE_ADDR = 0b01111100;
// static const uint8_t ST7591_COL_ADDR_H = 0b00010000;

}  // namespace st7567_base
}  // namespace esphome

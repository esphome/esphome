#pragma once

#include <stdint.h>

#ifdef USE_ESP32
#include "soc/gpio_struct.h"

// I2S signal indices for the ESP32 GPIO matrix (ESP32 only, I2S1).
#ifndef I2S1O_BCK_OUT_IDX
#include "soc/gpio_sig_map.h"
#endif
#endif  // USE_ESP32

namespace esphome::inkplate {

// Shared waveform LUTs for all parallel Inkplate boards.
// Source: InkplateLibrary/src/graphics/GraphicsDefs.h
// inline constexpr gives one definition across all TUs (no duplicate flash copies).
inline constexpr uint8_t INKPLATE_LUTB[16] = {0xFF, 0xFD, 0xF7, 0xF5, 0xDF, 0xDD, 0xD7, 0xD5,
                                              0x7F, 0x7D, 0x77, 0x75, 0x5F, 0x5D, 0x57, 0x55};

// LUTW — partial update white-pulse waveform (black→white transitions).
inline constexpr uint8_t INKPLATE_LUTW[16] = {0xFF, 0xFE, 0xFB, 0xFA, 0xEF, 0xEE, 0xEB, 0xEA,
                                              0xBF, 0xBE, 0xBB, 0xBA, 0xAF, 0xAE, 0xAB, 0xAA};

inline constexpr uint8_t INKPLATE_LUT2[16] = {0xAA, 0xA9, 0xA6, 0xA5, 0x9A, 0x99, 0x96, 0x95,
                                              0x6A, 0x69, 0x66, 0x65, 0x5A, 0x59, 0x56, 0x55};

// Shared GPIO assignments for all parallel Inkplate I2S boards.
// CKV = GPIO32, SPH = GPIO33, LE = GPIO2.
static constexpr uint8_t INK_GPIO_CKV = 32;
static constexpr uint8_t INK_GPIO_SPH = 33;
static constexpr uint8_t INK_GPIO_LE = 2;

#ifdef USE_ESP32
// Direct GPIO register macros for timing-critical vscan / send operations.
// CKV and SPH are in the GPIO1 bank (GPIOs 32-39); LE is in GPIO0 bank.
#define INK_CKV_SET() \
  do { \
    GPIO.out1_w1ts.val = (1u << (INK_GPIO_CKV - 32)); \
  } while (0)
#define INK_CKV_CLEAR() \
  do { \
    GPIO.out1_w1tc.val = (1u << (INK_GPIO_CKV - 32)); \
  } while (0)
#define INK_SPH_SET() \
  do { \
    GPIO.out1_w1ts.val = (1u << (INK_GPIO_SPH - 32)); \
  } while (0)
#define INK_SPH_CLEAR() \
  do { \
    GPIO.out1_w1tc.val = (1u << (INK_GPIO_SPH - 32)); \
  } while (0)
#define INK_LE_SET() \
  do { \
    GPIO.out_w1ts = (1u << INK_GPIO_LE); \
  } while (0)
#define INK_LE_CLEAR() \
  do { \
    GPIO.out_w1tc = (1u << INK_GPIO_LE); \
  } while (0)
#endif  // USE_ESP32

// TPS65186 power-good bitmask (all rails OK).
static constexpr uint8_t TPS_PWR_GOOD_OK = 0b11111010;

}  // namespace esphome::inkplate

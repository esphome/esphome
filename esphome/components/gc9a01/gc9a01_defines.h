#pragma once

namespace esphome {
namespace gc9a01 {

// Color definitions
// clang-format off
static const uint8_t GC9A01_MADCTL_MY   = 0x80;   ///< Bit 7 Bottom to top
static const uint8_t GC9A01_MADCTL_MX   = 0x40;   ///< Bit 6 Right to left
static const uint8_t GC9A01_MADCTL_MV   = 0x20;   ///< Bit 5 Reverse Mode
static const uint8_t GC9A01_MADCTL_ML   = 0x10;   ///< Bit 4 LCD refresh Bottom to top
static const uint8_t GC9A01_MADCTL_RGB  = 0x00;  ///< Bit 3 Red-Green-Blue pixel order
// clang-format on

static const uint8_t GC9A01_TFTWIDTH = 240;   ///< GC9A01 max TFT width
static const uint8_t GC9A01_TFTHEIGHT = 240;  ///< GC9A01 max TFT height

//delays
static const uint8_t GC9A01_RST_DELAY = 120;    ///< delay ms wait for reset finish
static const uint8_t GC9A01_SLPIN_DELAY = 120;  ///< delay ms wait for sleep in finish
static const uint8_t GC9A01_SLPOUT_DELAY = 120; ///< delay ms wait for sleep out finish

// All GC9A01 specific commands some are used by init()
static const uint8_t GC9A01_NOP = 0x00;
static const uint8_t GC9A01_SWRESET = 0x01;
static const uint8_t GC9A01_RDDID = 0x04;
static const uint8_t GC9A01_RDDST = 0x09;

static const uint8_t GC9A01_SLPIN = 0x10;
static const uint8_t GC9A01_SLPOUT = 0x11;
static const uint8_t GC9A01_PTLON = 0x12;
static const uint8_t GC9A01_NORON = 0x13;

static const uint8_t GC9A01_INVOFF = 0x20;
static const uint8_t GC9A01_INVON = 0x21;
static const uint8_t GC9A01_DISPOFF = 0x28;
static const uint8_t GC9A01_DISPON = 0x29;

static const uint8_t GC9A01_CASET = 0x2A;
static const uint8_t GC9A01_RASET = 0x2B;
static const uint8_t GC9A01_RAMWR = 0x2C;
static const uint8_t GC9A01_RAMRD = 0x2E;

static const uint8_t GC9A01_PTLAR = 0x30;
static const uint8_t GC9A01_COLMOD = 0x3A;
static const uint8_t GC9A01_MADCTL = 0x36;

static const uint8_t GC9A01_RDID1 = 0xDA;
static const uint8_t GC9A01_RDID2 = 0xDB;
static const uint8_t GC9A01_RDID3 = 0xDC;
static const uint8_t GC9A01_RDID4 = 0xDD;

}  // namespace gc9a01
}  // namespace esphome

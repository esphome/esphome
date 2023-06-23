#pragma once

namespace esphome {
namespace gc9a01 {

static const uint8_t GC9A01_TFTWIDTH = 240;   ///< Display width in pixels
static const uint8_t GC9A01_TFTHEIGHT = 240;  ///< Display height in pixels

// NOTE: ILI9341 registers defined (but commented out) are ones that
// *might* be compatible with the GC9A01, but aren't documented in
// the device datasheet. A couple are defined (with ILI name) and NOT
// commented out because they appeared in the manufacturer example code
// as raw register addresses, no documentation in datasheet, they SEEM
// to do the same thing as their ILI equivalents but this is not 100%
// confirmed so they've not been assigned GC9A01 register defines.

// static const uint8_t ILI9341_NOP = 0x00;     ///< No-op register
static const uint8_t GC9A01_SWRESET = 0x01;  ///< Software reset register

// static const uint8_t GC9A01 = 0x04;   ///< Read display identification information
static const uint8_t GC9A01_STATUS = 0x09;  ///< Read Display Status

static const uint8_t GC9A01_SLPIN = 0x10;   ///< Enter Sleep Mode
static const uint8_t GC9A01_SLPOUT = 0x11;  ///< Sleep Out
static const uint8_t GC9A01_PTLON = 0x12;   ///< Partial Mode ON
static const uint8_t GC9A01_NORON = 0x13;   ///< Normal Display Mode ON

static const uint8_t GC9A01_INVOFF = 0x20;   ///< Display Inversion OFF
static const uint8_t GC9A01_INVON = 0x21;    ///< Display Inversion ON
static const uint8_t GC9A01_DISPOFF = 0x28;  ///< Display OFF
static const uint8_t GC9A01_DISPON = 0x29;   ///< Display ON

static const uint8_t GC9A01_CASET = 0x2A;  ///< Column Address Set
static const uint8_t GC9A01_PASET = 0x2B;  ///< Page Address Set
static const uint8_t GC9A01_RAMWR = 0x2C;  ///< Memory Write
// static const uint8_t ILI9341_RAMRD = 0x2E; ///< Memory Read

static const uint8_t GC9A01_PTLAR = 0x30;     ///< Partial Area
static const uint8_t GC9A01_VSCRDEF = 0x33;   ///< Vertical Scrolling Definition
static const uint8_t GC9A01_TEOFF = 0x34;     ///< Tearing effect line off
static const uint8_t GC9A01_TEON = 0x35;      ///< Tearing effect line on
static const uint8_t GC9A01_MADCTL = 0x36;    ///< Memory Access Control
static const uint8_t GC9A01_VSCRSADD = 0x37;  ///< Vertical Scrolling Start Address
static const uint8_t GC9A01_PIXFMT = 0x3A;    ///< COLMOD: Pixel Format Set

static const uint8_t GC9A011_DFUNCTR = 0xB6;  ///< Display Function Control

static const uint8_t GC9A011_VREG1A = 0xC3;  ///< Vreg1a voltage control
static const uint8_t GC9A011_VREG1B = 0xC4;  ///< Vreg1b voltage control
static const uint8_t GC9A011_VREG2A = 0xC9;  ///< Vreg2a voltage control

static const uint8_t GC9A01_RDID1 = 0xDA;  ///< Read ID 1
static const uint8_t GC9A01_RDID2 = 0xDB;  ///< Read ID 2
static const uint8_t GC9A01_RDID3 = 0xDC;  ///< Read ID 3

// static const uint8_t ILI9341_GMCTRP1 = 0xE0; ///< Positive Gamma Correction
static const uint8_t GC9A01_GMCTRN1 = 0xE1;    ///< Negative Gamma Correction
static const uint8_t GC9A01_FRAMERATE = 0xE8;  ///< Frame rate control
static const uint8_t GC9A01_INREGEN2 = 0xEF;   ///< Inter register enable 2
static const uint8_t GC9A01_GAMMA1 = 0xF0;     ///< Set gamma 1
static const uint8_t GC9A01_GAMMA2 = 0xF1;     ///< Set gamma 2
static const uint8_t GC9A01_GAMMA3 = 0xF2;     ///< Set gamma 3
static const uint8_t GC9A01_GAMMA4 = 0xF3;     ///< Set gamma 4
static const uint8_t GC9A01_INREGEN1 = 0xFE;   ///< Inter register enable 1

}  // namespace gc9a01
}  // namespace esphome

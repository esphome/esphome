#pragma once

namespace esphome {
namespace ili9xxx {

// Color definitions
// clang-format off
static const uint8_t MADCTL_MY    = 0x80;   ///< Bit 7 Bottom to top
static const uint8_t MADCTL_MX    = 0x40;   ///< Bit 6 Right to left
static const uint8_t MADCTL_MV    = 0x20;   ///< Bit 5 Reverse Mode
static const uint8_t MADCTL_ML    = 0x10;   ///< Bit 4 LCD refresh Bottom to top
static const uint8_t MADCTL_RGB   = 0x00;  ///< Bit 3 Red-Green-Blue pixel order
static const uint8_t MADCTL_BGR   = 0x08;  ///< Bit 3 Blue-Green-Red pixel order
static const uint8_t MADCTL_MH    = 0x04;   ///< Bit 2 LCD refresh right to left
// clang-format on

// All ILI9XXX specific commands some are used by init()
static const uint8_t ILI9XXX_NOP = 0x00;
static const uint8_t ILI9XXX_SWRESET = 0x01;
static const uint8_t ILI9XXX_RDDID = 0x04;
static const uint8_t ILI9XXX_RDDST = 0x09;

static const uint8_t ILI9XXX_SLPIN = 0x10;
static const uint8_t ILI9XXX_SLPOUT = 0x11;
static const uint8_t ILI9XXX_PTLON = 0x12;
static const uint8_t ILI9XXX_NORON = 0x13;

static const uint8_t ILI9XXX_RDMODE = 0x0A;
static const uint8_t ILI9XXX_RDMADCTL = 0x0B;
static const uint8_t ILI9XXX_RDPIXFMT = 0x0C;
static const uint8_t ILI9XXX_RDIMGFMT = 0x0D;
static const uint8_t ILI9XXX_RDSELFDIAG = 0x0F;

static const uint8_t ILI9XXX_INVOFF = 0x20;
static const uint8_t ILI9XXX_INVON = 0x21;
static const uint8_t ILI9XXX_GAMMASET = 0x26;
static const uint8_t ILI9XXX_DISPOFF = 0x28;
static const uint8_t ILI9XXX_DISPON = 0x29;

static const uint8_t ILI9XXX_CASET = 0x2A;
static const uint8_t ILI9XXX_PASET = 0x2B;
static const uint8_t ILI9XXX_RAMWR = 0x2C;
static const uint8_t ILI9XXX_RAMRD = 0x2E;

static const uint8_t ILI9XXX_PTLAR = 0x30;
static const uint8_t ILI9XXX_VSCRDEF = 0x33;
static const uint8_t ILI9XXX_MADCTL = 0x36;
static const uint8_t ILI9XXX_VSCRSADD = 0x37;
static const uint8_t ILI9XXX_IDMOFF = 0x38;
static const uint8_t ILI9XXX_IDMON = 0x39;
static const uint8_t ILI9XXX_PIXFMT = 0x3A;
static const uint8_t ILI9XXX_COLMOD = 0x3A;

static const uint8_t ILI9XXX_GETSCANLINE = 0x45;

static const uint8_t ILI9XXX_WRDISBV = 0x51;
static const uint8_t ILI9XXX_RDDISBV = 0x52;
static const uint8_t ILI9XXX_WRCTRLD = 0x53;

static const uint8_t ILI9XXX_IFMODE = 0xB0;
static const uint8_t ILI9XXX_FRMCTR1 = 0xB1;
static const uint8_t ILI9XXX_FRMCTR2 = 0xB2;
static const uint8_t ILI9XXX_FRMCTR3 = 0xB3;
static const uint8_t ILI9XXX_INVCTR = 0xB4;
static const uint8_t ILI9XXX_DFUNCTR = 0xB6;
static const uint8_t ILI9XXX_ETMOD = 0xB7;

static const uint8_t ILI9XXX_PWCTR1 = 0xC0;
static const uint8_t ILI9XXX_PWCTR2 = 0xC1;
static const uint8_t ILI9XXX_PWCTR3 = 0xC2;
static const uint8_t ILI9XXX_PWCTR4 = 0xC3;
static const uint8_t ILI9XXX_PWCTR5 = 0xC4;
static const uint8_t ILI9XXX_VMCTR1 = 0xC5;
static const uint8_t ILI9XXX_IFCTR = 0xC6;
static const uint8_t ILI9XXX_VMCTR2 = 0xC7;
static const uint8_t ILI9XXX_GMCTR = 0xC8;
static const uint8_t ILI9XXX_SETEXTC = 0xC8;

static const uint8_t ILI9XXX_PWSET = 0xD0;
static const uint8_t ILI9XXX_VMCTR = 0xD1;
static const uint8_t ILI9XXX_PWSETN = 0xD2;
static const uint8_t ILI9XXX_RDID4 = 0xD3;
static const uint8_t ILI9XXX_RDINDEX = 0xD9;
static const uint8_t ILI9XXX_RDID1 = 0xDA;
static const uint8_t ILI9XXX_RDID2 = 0xDB;
static const uint8_t ILI9XXX_RDID3 = 0xDC;
static const uint8_t ILI9XXX_RDIDX = 0xDD;  // TBC

static const uint8_t ILI9XXX_GMCTRP1 = 0xE0;
static const uint8_t ILI9XXX_GMCTRN1 = 0xE1;

static const uint8_t ILI9XXX_CSCON = 0xF0;
static const uint8_t ILI9XXX_ADJCTL3 = 0xF7;

}  // namespace ili9xxx
}  // namespace esphome

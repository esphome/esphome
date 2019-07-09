#pragma once

namespace esphome {
namespace ili9341 {

// Color definitions
// clang-format off
static const uint16_t BLACK       = 0x0000;  ///<   0,   0,   0
static const uint16_t NAVY        = 0x000F;  ///<   0,   0, 123
static const uint16_t DARKGREEN   = 0x03E0;  ///<   0, 125,   0
static const uint16_t DARKCYAN    = 0x03EF;  ///<   0, 125, 123
static const uint16_t MAROON      = 0x7800;  ///< 123,   0,   0
static const uint16_t PURPLE      = 0x780F;  ///< 123,   0, 123
static const uint16_t OLIVE       = 0x7BE0;  ///< 123, 125,   0
static const uint16_t LIGHTGREY   = 0xC618;  ///< 198, 195, 198
static const uint16_t DARKGREY    = 0x7BEF;  ///< 123, 125, 123
static const uint16_t BLUE        = 0x001F;  ///<   0,   0, 255
static const uint16_t GREEN       = 0x07E0;  ///<   0, 255,   0
static const uint16_t CYAN        = 0x07FF;  ///<   0, 255, 255
static const uint16_t RED         = 0xF800;  ///< 255,   0,   0
static const uint16_t MAGENTA     = 0xF81F;  ///< 255,   0, 255
static const uint16_t YELLOW      = 0xFFE0;  ///< 255, 255,   0
static const uint16_t WHITE       = 0xFFFF;  ///< 255, 255, 255
static const uint16_t ORANGE      = 0xFD20;  ///< 255, 165,   0
static const uint16_t GREENYELLOW = 0xAFE5;  ///< 173, 255,  41
static const uint16_t PINK        = 0xFC18;  ///< 255, 130, 198

static const uint8_t MADCTL_MY    = 0x80;   ///< Bit 7 Bottom to top
static const uint8_t MADCTL_MX    = 0x40;   ///< Bit 6 Right to left
static const uint8_t MADCTL_MV    = 0x20;   ///< Bit 5 Reverse Mode
static const uint8_t MADCTL_ML    = 0x10;   ///< Bit 4 LCD refresh Bottom to top
static const uint8_t MADCTL_RGB   = 0x00;  ///< Bit 3 Red-Green-Blue pixel order
static const uint8_t MADCTL_BGR   = 0x08;  ///< Bit 3 Blue-Green-Red pixel order
static const uint8_t MADCTL_MH    = 0x04;   ///< Bit 2 LCD refresh right to left
// clang-format on

static const uint16_t ILI9341_TFTWIDTH = 320;   ///< ILI9341 max TFT width
static const uint16_t ILI9341_TFTHEIGHT = 240;  ///< ILI9341 max TFT height

// All ILI9341 specific commands some are used by init()
static const uint8_t ILI9341_NOP = 0x00;
static const uint8_t ILI9341_SWRESET = 0x01;
static const uint8_t ILI9341_RDDID = 0x04;
static const uint8_t ILI9341_RDDST = 0x09;

static const uint8_t ILI9341_SLPIN = 0x10;
static const uint8_t ILI9341_SLPOUT = 0x11;
static const uint8_t ILI9341_PTLON = 0x12;
static const uint8_t ILI9341_NORON = 0x13;

static const uint8_t ILI9341_RDMODE = 0x0A;
static const uint8_t ILI9341_RDMADCTL = 0x0B;
static const uint8_t ILI9341_RDPIXFMT = 0x0C;
static const uint8_t ILI9341_RDIMGFMT = 0x0A;
static const uint8_t ILI9341_RDSELFDIAG = 0x0F;

static const uint8_t ILI9341_INVOFF = 0x20;
static const uint8_t ILI9341_INVON = 0x21;
static const uint8_t ILI9341_GAMMASET = 0x26;
static const uint8_t ILI9341_DISPOFF = 0x28;
static const uint8_t ILI9341_DISPON = 0x29;

static const uint8_t ILI9341_CASET = 0x2A;
static const uint8_t ILI9341_PASET = 0x2B;
static const uint8_t ILI9341_RAMWR = 0x2C;
static const uint8_t ILI9341_RAMRD = 0x2E;

static const uint8_t ILI9341_PTLAR = 0x30;
static const uint8_t ILI9341_VSCRDEF = 0x33;
static const uint8_t ILI9341_MADCTL = 0x36;
static const uint8_t ILI9341_VSCRSADD = 0x37;
static const uint8_t ILI9341_PIXFMT = 0x3A;

static const uint8_t ILI9341_WRDISBV = 0x51;
static const uint8_t ILI9341_RDDISBV = 0x52;
static const uint8_t ILI9341_WRCTRLD = 0x53;

static const uint8_t ILI9341_FRMCTR1 = 0xB1;
static const uint8_t ILI9341_FRMCTR2 = 0xB2;
static const uint8_t ILI9341_FRMCTR3 = 0xB3;
static const uint8_t ILI9341_INVCTR = 0xB4;
static const uint8_t ILI9341_DFUNCTR = 0xB6;

static const uint8_t ILI9341_PWCTR1 = 0xC0;
static const uint8_t ILI9341_PWCTR2 = 0xC1;
static const uint8_t ILI9341_PWCTR3 = 0xC2;
static const uint8_t ILI9341_PWCTR4 = 0xC3;
static const uint8_t ILI9341_PWCTR5 = 0xC4;
static const uint8_t ILI9341_VMCTR1 = 0xC5;
static const uint8_t ILI9341_VMCTR2 = 0xC7;

static const uint8_t ILI9341_RDID4 = 0xD3;
static const uint8_t ILI9341_RDINDEX = 0xD9;
static const uint8_t ILI9341_RDID1 = 0xDA;
static const uint8_t ILI9341_RDID2 = 0xDB;
static const uint8_t ILI9341_RDID3 = 0xDC;
static const uint8_t ILI9341_RDIDX = 0xDD;  // TBC

static const uint8_t ILI9341_GMCTRP1 = 0xE0;
static const uint8_t ILI9341_GMCTRN1 = 0xE1;

}  // namespace ili9341
}  // namespace esphome
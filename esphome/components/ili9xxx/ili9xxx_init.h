#pragma once
#include "esphome/core/helpers.h"

namespace esphome {
namespace ili9xxx {

// clang-format off
static const uint8_t PROGMEM INITCMD_M5STACK[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9XXX_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9XXX_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9XXX_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9XXX_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9XXX_MADCTL  , 1, MADCTL_BGR,       // Memory Access Control
  ILI9XXX_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9XXX_PIXFMT  , 1, 0x55,
  ILI9XXX_FRMCTR1 , 2, 0x00, 0x13,
  ILI9XXX_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9XXX_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9XXX_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Set Gamma
                        0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03,
                        0x0E, 0x09, 0x00,
  ILI9XXX_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Set Gamma
                        0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C,
                        0x31, 0x36, 0x0F,
  ILI9XXX_SLPOUT  , 0x80,                // Exit Sleep
  ILI9XXX_DISPON  , 0x80,                // Display on
  0x00                                   // End of list
};

static const uint8_t PROGMEM INITCMD_M5CORE[] = {
  ILI9XXX_SETEXTC, 3, 0xFF,0x93,0x42,   // Turn on the external command
  ILI9XXX_PWCTR1 , 2, 0x12, 0x12,
  ILI9XXX_PWCTR2 , 1, 0x03,
  ILI9XXX_VMCTR1 , 1, 0xF2,
  ILI9XXX_IFMODE , 1, 0xE0,
  0xF6           , 3, 0x01, 0x00, 0x00,
  ILI9XXX_GMCTRP1,15, 0x00,0x0C,0x11,0x04,0x11,0x08,0x37,0x89,0x4C,0x06,0x0C,0x0A,0x2E,0x34,0x0F,
  ILI9XXX_GMCTRN1,15, 0x00,0x0B,0x11,0x05,0x13,0x09,0x33,0x67,0x48,0x07,0x0E,0x0B,0x2E,0x33,0x0F,
  ILI9XXX_DFUNCTR, 4, 0x08,0x82,0x1D,0x04,
  ILI9XXX_IDMOFF , 0,
  ILI9XXX_DISPON , 0x80,                // Display on
  ILI9XXX_SLPOUT , 0x80,                // Exit Sleep

  0x00                                   // End of list
};



static const uint8_t PROGMEM INITCMD_ILI9341[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9XXX_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9XXX_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9XXX_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9XXX_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9XXX_MADCTL  , 1, 0x48,             // Memory Access Control
  ILI9XXX_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9XXX_PIXFMT  , 1, 0x55,
  ILI9XXX_FRMCTR1 , 2, 0x00, 0x18,
  ILI9XXX_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9XXX_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9XXX_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Set Gamma
                        0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03,
                        0x0E, 0x09, 0x00,
  ILI9XXX_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Set Gamma
                        0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C,
                        0x31, 0x36, 0x0F,
  ILI9XXX_SLPOUT  , 0x80,                // Exit Sleep
  ILI9XXX_DISPON  , 0x80,                // Display on
  0x00                                   // End of list
};

static const uint8_t PROGMEM INITCMD_ILI9481[] = {
  ILI9XXX_SLPOUT ,  0x80,    // Exit sleep mode
  ILI9XXX_PWSET  , 3, 0x07, 0x41, 0x1D,
  ILI9XXX_VMCTR  , 3, 0x00, 0x1C, 0x1F,
  ILI9XXX_PWSETN , 2, 0x01, 0x11,
  ILI9XXX_PWCTR1 , 5, 0x10, 0x3B, 0x00, 0x02, 0x11,
  ILI9XXX_VMCTR1 , 1, 0x03,
  ILI9XXX_IFCTR  , 1, 0x83,
  ILI9XXX_GMCTR  ,12, 0x00, 0x26, 0x21, 0x00, 0x00, 0x1F, 0x65, 0x23, 0x77, 0x00, 0x0F, 0x00,
  ILI9XXX_IFMODE , 1, 0x00,  // CommandAccessProtect
  ILI9XXX_PTLAR , 4, 0, 0, 1, 0xDF,
  0xE4        , 1, 0xA0,
  ILI9XXX_MADCTL  , 1, MADCTL_MV | MADCTL_BGR,       // Memory Access Control
  ILI9XXX_CSCON , 1, 0x01,
  ILI9XXX_PIXFMT, 1, 0x55,  // 16 bit mode
  ILI9XXX_INVON, 0,
  ILI9XXX_DISPON, 0x80,     // Set display on
  0x00 // end
};

static const uint8_t PROGMEM INITCMD_ILI9481_18[] = {
    ILI9XXX_SLPOUT ,  0x80,    // Exit sleep mode
    ILI9XXX_PWSET  , 3, 0x07, 0x41, 0x1D,
    ILI9XXX_VMCTR  , 3, 0x00, 0x1C, 0x1F,
    ILI9XXX_PWSETN , 2, 0x01, 0x11,
    ILI9XXX_PWCTR1 , 5, 0x10, 0x3B, 0x00, 0x02, 0x11,
    ILI9XXX_VMCTR1 , 1, 0x03,
    ILI9XXX_IFCTR  , 1, 0x83,
    ILI9XXX_GMCTR  ,12, 0x00, 0x26, 0x21, 0x00, 0x00, 0x1F, 0x65, 0x23, 0x77, 0x00, 0x0F, 0x00,
    ILI9XXX_IFMODE , 1, 0x00,  // CommandAccessProtect
    ILI9XXX_PTLAR , 4, 0, 0, 1, 0xDF,
    0xE4        , 1, 0xA0,
    ILI9XXX_MADCTL  , 1, MADCTL_MX| MADCTL_BGR,       // Memory Access Control
    ILI9XXX_CSCON , 1, 0x01,
    ILI9XXX_PIXFMT, 1, 0x66,  // 18 bit mode
    ILI9XXX_INVON, 0,
    ILI9XXX_DISPON, 0x80,     // Set display on
    0x00 // end
};

static const uint8_t PROGMEM INITCMD_ILI9486[] = {
  ILI9XXX_SLPOUT, 0x80,
  ILI9XXX_PIXFMT, 1, 0x55,
  ILI9XXX_PWCTR3, 1, 0x44,
  ILI9XXX_VMCTR1, 4, 0x00, 0x00, 0x00, 0x00,
  ILI9XXX_GMCTRP1, 15, 0x0f,0x1f,0x1c,0x0c,0x0f,0x08,0x48,0x98,0x37,0x0a,0x13,0x04,0x11,0x0d,0x00,
  ILI9XXX_GMCTRN1, 15, 0x0f,0x32,0x2e,0x0b,0x0d,0x05,0x47,0x75,0x37,0x06,0x10,0x03,0x24,0x20,0x00,
  ILI9XXX_INVOFF, 0x80,
  ILI9XXX_MADCTL, 1, 0x48,
  ILI9XXX_DISPON, 0x80,

  // ILI9XXX_MADCTL, 1, MADCTL_BGR | MADCTL_MV, //hardware rotation
  0x00                                   // End of list
};

static const uint8_t PROGMEM INITCMD_ILI9488[] = {
  ILI9XXX_GMCTRP1,15, 0x0f, 0x24, 0x1c, 0x0a, 0x0f, 0x08, 0x43, 0x88, 0x32, 0x0f, 0x10, 0x06, 0x0f, 0x07, 0x00,
  ILI9XXX_GMCTRN1,15, 0x0F, 0x38, 0x30, 0x09, 0x0f, 0x0f, 0x4e, 0x77, 0x3c, 0x07, 0x10, 0x05, 0x23, 0x1b, 0x00,

  ILI9XXX_PWCTR1,  2, 0x17, 0x15,  // VRH1 VRH2
  ILI9XXX_PWCTR2,  1, 0x41,  // VGH, VGL
  ILI9XXX_VMCTR1,  3, 0x00, 0x12, 0x80,    // nVM VCM_REG VCM_REG_EN

  ILI9XXX_IFMODE,  1, 0x00,
  ILI9XXX_FRMCTR1, 1, 0xA0,  // Frame rate = 60Hz
  ILI9XXX_INVCTR,  1, 0x02,  // Display Inversion Control = 2dot

  ILI9XXX_DFUNCTR, 2, 0x02, 0x02, // Nomal scan

  0xE9, 1, 0x00,   // Set Image Functio. Disable 24 bit data

  ILI9XXX_ADJCTL3, 4, 0xA9, 0x51, 0x2C, 0x82,  // Adjust Control 3

  ILI9XXX_MADCTL,  1, 0x28,
  //ILI9XXX_PIXFMT,  1, 0x55,  // Interface Pixel Format = 16bit
  ILI9XXX_PIXFMT, 1, 0x66,   //ILI9488 only supports 18-bit pixel format in 4/3 wire SPI mode



  // 5 frames
  //ILI9XXX_ETMOD,   1, 0xC6,  //


  ILI9XXX_SLPOUT,  0x80,    // Exit sleep mode
  //ILI9XXX_INVON  , 0,
  ILI9XXX_DISPON,  0x80,    // Set display on
  0x00 // end
};

static const uint8_t PROGMEM INITCMD_ILI9488_A[] = {
  ILI9XXX_GMCTRP1,15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
  ILI9XXX_GMCTRN1,15, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,

  ILI9XXX_PWCTR1,  2, 0x17, 0x15,  // VRH1 VRH2
  ILI9XXX_PWCTR2,  1, 0x41,  // VGH, VGL
  ILI9XXX_VMCTR1,  3, 0x00, 0x12, 0x80,    // nVM VCM_REG VCM_REG_EN

  ILI9XXX_IFMODE,  1, 0x00,
  ILI9XXX_FRMCTR1, 1, 0xA0,  // Frame rate = 60Hz
  ILI9XXX_INVCTR,  1, 0x02,  // Display Inversion Control = 2dot

  ILI9XXX_DFUNCTR, 2, 0x02, 0x02, // Nomal scan

  0xE9, 1, 0x00,   // Set Image Functio. Disable 24 bit data

  ILI9XXX_ADJCTL3, 4, 0xA9, 0x51, 0x2C, 0x82,  // Adjust Control 3

  ILI9XXX_MADCTL,  1, 0x28,
  //ILI9XXX_PIXFMT,  1, 0x55,  // Interface Pixel Format = 16bit
  ILI9XXX_PIXFMT, 1, 0x66,   //ILI9488 only supports 18-bit pixel format in 4/3 wire SPI mode



  // 5 frames
  //ILI9XXX_ETMOD,   1, 0xC6,  //


  ILI9XXX_SLPOUT,  0x80,    // Exit sleep mode
  //ILI9XXX_INVON  , 0,
  ILI9XXX_DISPON,  0x80,    // Set display on
  0x00 // end
};

static const uint8_t PROGMEM INITCMD_ST7796[] = {
  // This ST7796S initilization routine was copied from https://github.com/prenticedavid/Adafruit_ST7796S_kbv/blob/master/Adafruit_ST7796S_kbv.cpp
  ILI9XXX_SWRESET, 0x80,         // Soft reset, then delay 150 ms
  ILI9XXX_CSCON, 1, 0xC3,              // ?? Unlock Manufacturer
  ILI9XXX_CSCON, 1, 0x96,
  ILI9XXX_VMCTR1, 1, 0x1C,              //VCOM  Control 1 [1C]
  ILI9XXX_MADCTL, 1, 0x48,              //Memory Access [00]
  ILI9XXX_PIXFMT, 1, 0x55,              //565
  ILI9XXX_IFMODE, 1, 0x80,              //Interface     [00]
  ILI9XXX_INVCTR, 1, 0x01,              //Inversion Control [01]
  ILI9XXX_DFUNCTR, 3, 0x80, 0x02, 0x3B,  // Display Function Control [80 02 3B] .kbv SS=1, NL=480
  ILI9XXX_ETMOD, 1, 0xC6,              //Entry Mode      [06]

  ILI9XXX_CSCON, 1, 0x69,              //?? lock manufacturer commands
  ILI9XXX_CSCON, 1, 0x3C,              //
  ILI9XXX_SLPOUT, 0x80, // Exit Sleep, then delay 150 ms
  ILI9XXX_DISPON, 0x80, // Main screen turn on, delay 150 ms
  0x00                                   // End of list
};

static const uint8_t PROGMEM INITCMD_S3BOX[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9XXX_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9XXX_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9XXX_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9XXX_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9XXX_MADCTL  , 1, 0xC8,             // Memory Access Control
  ILI9XXX_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9XXX_PIXFMT  , 1, 0x55,
  ILI9XXX_FRMCTR1 , 2, 0x00, 0x18,
  ILI9XXX_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9XXX_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9XXX_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Set Gamma
                        0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03,
                        0x0E, 0x09, 0x00,
  ILI9XXX_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Set Gamma
                        0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C,
                        0x31, 0x36, 0x0F,
  ILI9XXX_SLPOUT  , 0x80,                // Exit Sleep
  ILI9XXX_DISPON  , 0x80,                // Display on
  0x00                                   // End of list
};

static const uint8_t PROGMEM INITCMD_S3BOXLITE[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9XXX_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9XXX_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9XXX_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9XXX_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9XXX_MADCTL  , 1, 0x40,             // Memory Access Control
  ILI9XXX_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9XXX_PIXFMT  , 1, 0x55,
  ILI9XXX_FRMCTR1 , 2, 0x00, 0x18,
  ILI9XXX_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9XXX_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9XXX_GMCTRP1 , 14, 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, // Set Gamma
                        0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14,
                        0x18, 0x1B,
  ILI9XXX_GMCTRN1 , 14, 0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, // Set Gamma
                        0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14,
                        0x17, 0x1B,
  ILI9XXX_SLPOUT  , 0x80,                // Exit Sleep
  ILI9XXX_DISPON  , 0x80,                // Display on
  0x00                                   // End of list
};

// clang-format on
}  // namespace ili9xxx
}  // namespace esphome

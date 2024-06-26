#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome {
namespace st7789v {

static const uint8_t ST7789_NOP = 0x00;        // No Operation
static const uint8_t ST7789_SWRESET = 0x01;    // Software Reset
static const uint8_t ST7789_RDDID = 0x04;      // Read Display ID
static const uint8_t ST7789_RDDST = 0x09;      // Read Display Status
static const uint8_t ST7789_RDDPM = 0x0A;      // Read Display Power Mode
static const uint8_t ST7789_RDDMADCTL = 0x0B;  // Read Display MADCTL
static const uint8_t ST7789_RDDCOLMOD = 0x0C;  // Read Display Pixel Format
static const uint8_t ST7789_RDDIM = 0x0D;      // Read Display Image Mode
static const uint8_t ST7789_RDDSM = 0x0E;      // Read Display Signal Mod
static const uint8_t ST7789_RDDSDR = 0x0F;     // Read Display Self-Diagnostic Result
static const uint8_t ST7789_SLPIN = 0x10;      // Sleep in
static const uint8_t ST7789_SLPOUT = 0x11;     // Sleep Out
static const uint8_t ST7789_PTLON = 0x12;      // Partial Display Mode On
static const uint8_t ST7789_NORON = 0x13;      // Normal Display Mode On
static const uint8_t ST7789_INVOFF = 0x20;     // Display Inversion Off
static const uint8_t ST7789_INVON = 0x21;      // Display Inversion On
static const uint8_t ST7789_GAMSET = 0x26;     // Gamma Set
static const uint8_t ST7789_DISPOFF = 0x28;    // Display Off
static const uint8_t ST7789_DISPON = 0x29;     // Display On
static const uint8_t ST7789_CASET = 0x2A;      // Column Address Set
static const uint8_t ST7789_RASET = 0x2B;      // Row Address Set
static const uint8_t ST7789_RAMWR = 0x2C;      // Memory Write
static const uint8_t ST7789_RAMRD = 0x2E;      // Memory Read
static const uint8_t ST7789_PTLAR = 0x30;      // Partial Area
static const uint8_t ST7789_VSCRDEF = 0x33;    // Vertical Scrolling Definition
static const uint8_t ST7789_TEOFF = 0x34;      // Tearing Effect Line Off
static const uint8_t ST7789_TEON = 0x35;       // Tearing Effect Line On
static const uint8_t ST7789_MADCTL = 0x36;     // Memory Data Access Control
static const uint8_t ST7789_VSCSAD = 0x37;     // Vertical Scroll Start Address of RAM
static const uint8_t ST7789_IDMOFF = 0x38;     // Idle Mode Off
static const uint8_t ST7789_IDMON = 0x39;      // Idle Mode On
static const uint8_t ST7789_COLMOD = 0x3A;     // Interface Pixel Format
static const uint8_t ST7789_WRMEMC = 0x3C;     // Write Memory Continue
static const uint8_t ST7789_RDMEMC = 0x3E;     // Read Memory Continue
static const uint8_t ST7789_STE = 0x44;        // Set Tear Scanline
static const uint8_t ST7789_GSCAN = 0x45;      // Get Scanline
static const uint8_t ST7789_WRDISBV = 0x51;    // Write Display Brightness
static const uint8_t ST7789_RDDISBV = 0x52;    // Read Display Brightness Value
static const uint8_t ST7789_WRCTRLD = 0x53;    // Write CTRL Display
static const uint8_t ST7789_RDCTRLD = 0x54;    // Read CTRL Value Display
static const uint8_t ST7789_WRCACE = 0x55;     // Write Content Adaptive Brightness Control and Color Enhancement
static const uint8_t ST7789_RDCABC = 0x56;     // Read Content Adaptive Brightness Control
static const uint8_t ST7789_WRCABCMB = 0x5E;   // Write CABC Minimum Brightnes
static const uint8_t ST7789_RDCABCMB = 0x5F;   // Read CABC Minimum Brightnes
static const uint8_t ST7789_RDABCSDR = 0x68;   // Read Automatic Brightness Control Self-Diagnostic Result
static const uint8_t ST7789_RDID1 = 0xDA;      // Read ID1
static const uint8_t ST7789_RDID2 = 0xDB;      // Read ID2
static const uint8_t ST7789_RDID3 = 0xDC;      // Read ID3
static const uint8_t ST7789_RAMCTRL = 0xB0;    // RAM Control
static const uint8_t ST7789_RGBCTRL = 0xB1;    // RGB Interface Control
static const uint8_t ST7789_PORCTRL = 0xB2;    // Porch Setting
static const uint8_t ST7789_FRCTRL1 = 0xB3;    // Frame Rate Control 1 (In partial mode/ idle colors)
static const uint8_t ST7789_PARCTRL = 0xB5;    // Partial mode Control
static const uint8_t ST7789_GCTRL = 0xB7;      // Gate Control
static const uint8_t ST7789_GTADJ = 0xB8;      // Gate On Timing Adjustment
static const uint8_t ST7789_DGMEN = 0xBA;      // Digital Gamma Enable
static const uint8_t ST7789_VCOMS = 0xBB;      // VCOMS Setting
static const uint8_t ST7789_LCMCTRL = 0xC0;    // LCM Control
static const uint8_t ST7789_IDSET = 0xC1;      // ID Code Setting
static const uint8_t ST7789_VDVVRHEN = 0xC2;   // VDV and VRH Command Enable
static const uint8_t ST7789_VRHS = 0xC3;       // VRH Set
static const uint8_t ST7789_VDVS = 0xC4;       // VDV Set
static const uint8_t ST7789_VCMOFSET = 0xC5;   // VCOMS Offset Set
static const uint8_t ST7789_FRCTRL2 = 0xC6;    // Frame Rate Control in Normal Mode
static const uint8_t ST7789_CABCCTRL = 0xC7;   // CABC Control
static const uint8_t ST7789_REGSEL1 = 0xC8;    // Register Value Selection 1
static const uint8_t ST7789_REGSEL2 = 0xCA;    // Register Value Selection
static const uint8_t ST7789_PWMFRSEL = 0xCC;   // PWM Frequency Selection
static const uint8_t ST7789_PWCTRL1 = 0xD0;    // Power Control 1
static const uint8_t ST7789_VAPVANEN = 0xD2;   // Enable VAP/VAN signal output
static const uint8_t ST7789_CMD2EN = 0xDF;     // Command 2 Enable
static const uint8_t ST7789_PVGAMCTRL = 0xE0;  // Positive Voltage Gamma Control
static const uint8_t ST7789_NVGAMCTRL = 0xE1;  // Negative Voltage Gamma Control
static const uint8_t ST7789_DGMLUTR = 0xE2;    // Digital Gamma Look-up Table for Red
static const uint8_t ST7789_DGMLUTB = 0xE3;    // Digital Gamma Look-up Table for Blue
static const uint8_t ST7789_GATECTRL = 0xE4;   // Gate Control
static const uint8_t ST7789_SPI2EN = 0xE7;     // SPI2 Enable
static const uint8_t ST7789_PWCTRL2 = 0xE8;    // Power Control 2
static const uint8_t ST7789_EQCTRL = 0xE9;     // Equalize time control
static const uint8_t ST7789_PROMCTRL = 0xEC;   // Program Mode Control
static const uint8_t ST7789_PROMEN = 0xFA;     // Program Mode Enable
static const uint8_t ST7789_NVMSET = 0xFC;     // NVM Setting
static const uint8_t ST7789_PROMACT = 0xFE;    // Program action

// Flags for ST7789_MADCTL
static const uint8_t ST7789_MADCTL_MY = 0x80;
static const uint8_t ST7789_MADCTL_MX = 0x40;
static const uint8_t ST7789_MADCTL_MV = 0x20;
static const uint8_t ST7789_MADCTL_ML = 0x10;
static const uint8_t ST7789_MADCTL_RGB = 0x00;
static const uint8_t ST7789_MADCTL_BGR = 0x08;
static const uint8_t ST7789_MADCTL_MH = 0x04;
static const uint8_t ST7789_MADCTL_SS = 0x02;
static const uint8_t ST7789_MADCTL_GS = 0x01;

static const uint8_t ST7789_MADCTL_COLOR_ORDER = ST7789_MADCTL_BGR;

class ST7789V : public display::DisplayBuffer,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_20MHZ> {
 public:
  void set_model_str(const char *model_str);
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_backlight_pin(GPIOPin *backlight_pin) { this->backlight_pin_ = backlight_pin; }
#ifdef USE_POWER_SUPPLY
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  void set_eightbitcolor(bool eightbitcolor) { this->eightbitcolor_ = eightbitcolor; }
  void set_height(uint32_t height) { this->height_ = height; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_offset_height(uint32_t offset_height) { this->offset_height_ = offset_height; }
  void set_offset_width(uint16_t offset_width) { this->offset_width_ = offset_width; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void write_display_data();

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *backlight_pin_{nullptr};
#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_;
#endif

  bool eightbitcolor_{false};
  uint16_t height_{0};
  uint16_t width_{0};
  uint16_t offset_height_{0};
  uint16_t offset_width_{0};

  void init_reset_();
  void backlight_(bool onoff);
  void write_command_(uint8_t value);
  void write_data_(uint8_t value);
  void write_addr_(uint16_t addr1, uint16_t addr2);
  void write_color_(uint16_t color, uint16_t size);

  int get_height_internal() override { return this->height_; }
  int get_width_internal() override { return this->width_; }
  size_t get_buffer_length_();

  void draw_filled_rect_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  const char *model_str_;
};

}  // namespace st7789v
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/defines.h"
#include "gdew0154m09_defines.h"

namespace esphome {
namespace gdew0154m09 {
enum {
  GDEW0154M09_WIDTH = 200,
  GDEW0154M09_HEIGHT = 200,
};

static const char *const TAG = "gdew0154m09";

class GDEW0154M09 : public PollingComponent,
                    public display::DisplayBuffer,
                    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                          spi::DATA_RATE_10MHZ> {
 public:
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }

  void dump_config() override;
  void update() override;
  void on_safe_shutdown() override;
  void setup() override {
    this->setup_pins_();
    this->initialize_();
  }

 protected:
  GPIOPin *reset_pin_;
  GPIOPin *dc_pin_;
  GPIOPin *busy_pin_;

  void initialize_();
  void setup_pins_();
  void HOT display_();
  int clear_(int mode);
  void reset_display_controller_();
  void data_(uint8_t data);
  void command_(uint8_t cmd);
  int write_init_list_(const unsigned char *list);

  uint8_t *lastbuff_ = nullptr;
  int get_width_internal() override { return GDEW0154M09_WIDTH; };
  int get_height_internal() override { return GDEW0154M09_HEIGHT; };
  size_t get_buffer_length_();
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void set_draw_addr_(uint16_t posx, uint16_t posy, uint16_t width, uint16_t height);
  void draw_buff_(uint8_t *lastbuff, uint8_t *buff, size_t size);

  int clear_dsram_();

  bool wait_until_idle_(uint32_t timeout = 1000);
  void deep_sleep_();
  void power_hv_on_();
};

// clang-format off
const unsigned char WFT0154CZB3_LIST[] = {
    11,                                                //  11 commands in list
    CMD_PSR_PANEL_SETTING, 2, 0xdf, 0x0e,              //  200x200, LUT from OTP, B/W mode, scan up, shift rt
    CMD_UNDOCUMENTED_0x4D, 1, 0x55,
    CMD_UNDOCUMENTED_0xAA, 1, 0x0f,
    CMD_UNDOCUMENTED_0xE9, 1, 0x02,
    CMD_UNDOCUMENTED_0xB6, 1, 0x11,
    CMD_UNDOCUMENTED_0xF3, 1, 0x0a,
    CMD_TRES_RESOLUTION_SETTING, 3, 0xc8, 0x00, 0xc8,
    CMD_TCON_TCONSETTING, 1, 0x00,
    CMD_CDI_VCOM_DATA_INTERVAL, 1, 0xd7,
    CMD_PWS_POWER_SAVING, 1, 0x00,
    CMD_PON_POWER_ON, 0
};
// clang-format on

// LUTs provided for future reference - currently unused.
const unsigned char LUT_VCOM_D_C1[] PROGMEM = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char LUT_WW1[] PROGMEM = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char LUT_BW1[] PROGMEM = {
    0x01, 0x84, 0x84, 0x83, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char LUT_WB1[] PROGMEM = {
    0x01, 0x44, 0x44, 0x43, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char LUT_BB1[] PROGMEM = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
}  // namespace gdew0154m09
}  // namespace esphome

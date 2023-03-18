#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace gdew0154m09 {
enum {
  HIGH = true,
  LOW = false,
  GDEW0154M09_WIDTH = 200,
  GDEW0154M09_HEIGHT = 200,
};

static const char *const TAG = "gdew0154m09";

class GDEW0154M09 : public PollingComponent,
                    public display::DisplayBuffer,
                    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                          spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_10MHZ> {
 public:
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void set_dc_pin(GPIOPin *dc_pin) {    this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset) {  this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }

  void dump_config() override;
  void update() override;
  void on_safe_shutdown() override;
  void setup() override {
    this->setup_pins_();
    this->initialize();
  }

 protected:
  GPIOPin *reset_pin_;
  GPIOPin *dc_pin_;
  GPIOPin *busy_pin_;

  void initialize();
  void setup_pins_();
  void HOT display();
  int clear(int mode);
  void resetDisplayController();
  void data(uint8_t data);
  void command(uint8_t cmd);
  int writeInitList(const unsigned char *list);

  uint8_t* _lastbuff = nullptr;
  int get_width_internal() override { return GDEW0154M09_WIDTH; };
  int get_height_internal() override { return GDEW0154M09_HEIGHT; };
  size_t get_buffer_length_();
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void setDrawAddr(uint16_t posx, uint16_t posy, uint16_t width, uint16_t height);
  void drawBuff(uint8_t *lastbuff, uint8_t *buff, size_t size);

  int clearDSRAM();

  bool wait_until_idle_(uint32_t timeout = 1000);
  void deep_sleep();
  void powerHVON();
};

const unsigned char WFT0154CZB3_LIST[] = {
    11,                                                                                     // 11 commands in list:
    0x00, 2, 0xdf, 0x0e,                                                                    // panel setting
    0x4D, 1, 0x55,                                                                          // FITIinternal code
    0xaa, 1, 0x0f,
    0xe9, 1, 0x02,
    0xb6, 1, 0x11,
    0xf3, 1, 0x0a,
    0x61, 3, 0xc8, 0x00, 0xc8,  // resolution setting
    0x60, 1, 0x00,                                                                          // Tcon setting
    0x50, 1, 0xd7,
    0xe3, 1, 0x00,
    0x04, 0                                                   // Power on
};

// LUTs provided for future reference - currently unused.
const unsigned char lut_vcomDC1[] PROGMEM = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char lut_ww1[] PROGMEM = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char lut_bw1[] PROGMEM = {
    0x01, 0x84, 0x84, 0x83, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char lut_wb1[] PROGMEM = {
    0x01, 0x44, 0x44, 0x43, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char lut_bb1[] PROGMEM = {
    0x01, 0x04, 0x04, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
} // namespace gdew0154m09
}  // namespace esphome

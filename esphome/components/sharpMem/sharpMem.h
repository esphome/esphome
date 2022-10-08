#pragma once

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sharpMem {

#define SHARPMEM_BIT_WRITECMD (0x01) // 0x80 in LSB format
#define SHARPMEM_BIT_VCOM (0x02)     // 0x40 in LSB format
#define SHARPMEM_BIT_CLEAR (0x04)    // 0x20 in LSB format

class SharpMem;

using sharpMem_writer_t = std::function<void(SharpMem &)>;

class SharpMem : public PollingComponent,
               public display::DisplayBuffer,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                     spi::DATA_RATE_200KHZ> {
 public:
  void set_writer(sharpMem_writer_t &&writer) { this->writer_local_ = writer; }
  void set_height(uint16_t height) { this->height_ = height; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_cs(GPIOPin *cs) { cs_ = cs; }
  void set_extmode_pin(GPIOPin *extmode) { extmode_ = extmode; }
  void set_extcomin_pin(GPIOPin *extcomin) { extcomin_ = extcomin; }
  void set_disp_pin(GPIOPin *disp) { disp_ = disp; }
  void set_sharp5v_on_pin(GPIOPin *sharp5v_on) { sharp5v_on_ = sharp5v_on; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void update() override;
  void fill(Color color) override;
  void write_display_data();

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  void display_init_();
  void command_(uint8_t value);
  void data_(uint8_t value);
  void send_(uint8_t type, uint8_t value);
  void start_transaction_();
  void end_transaction_();

  uint8_t width_ = 400, height_ = 240;
  GPIOPin *cs_;
  GPIOPin *extmode_;
  GPIOPin *extcomin_;
  GPIOPin *disp_;
  GPIOPin *sharp5v_on_;
  optional<sharpMem_writer_t> writer_local_{};

  uint8_t _sharpmem_vcom;
};

}  // namespace sharpMem
}  // namespace esphome

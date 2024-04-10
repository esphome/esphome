#pragma once
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/rect.h"
#include "ili9xxx_defines.h"
#include "ili9xxx_init.h"

namespace esphome {
namespace ili9xxx {


class IOBus {
 public:
  virtual void setup(uint16_t width, uint16_t height){};

  virtual void begin_commands(){};
  virtual void end_commands(){};
  inline void send_command(uint8_t command) { this->send_command(command, nullptr, 0); }
  void send_command(uint8_t command, const uint8_t *data, size_t len);
  inline void send_data(const uint8_t data) { this->send_data(&data, 1); }
  void send_data(const uint8_t *data, size_t len = 1);

  virtual void begin_pixels() { this->begin_commands(); }
  virtual void end_pixels() { this->end_commands(); }
  void send_pixels(Rect window, const uint8_t *data) { this->send_pixels(window, data, 1); }
  virtual void send_pixels(Rect window, const uint8_t *data, size_t len) { this->send_data(data, len); }

  virtual void dump_config(){};

 protected:
  virtual void start_command(){};
  virtual void end_command(){};
  virtual void start_data(){};
  virtual void end_data(){};

  virtual void command(uint8_t value);
  virtual void data(const uint8_t *value, size_t len);
  virtual void send_byte(uint8_t data) = 0;
  virtual void send_array(const uint8_t *data, size_t len) = 0;

  bool always_begin_transaction_{false};
};

class SPIBus : public IOBus,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_40MHZ> {
 public:
  void setup(uint16_t width, uint16_t height) override;
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }

  void begin_commands() override;
  void end_commands() override;

  void dump_config() override;

 protected:
  void start_command() override;
  void end_command() override;
  void start_data() override;

  void send_byte(uint8_t data) override;
  void send_array(const uint8_t *data, size_t len) override;
  GPIOPin *dc_pin_{nullptr};
  int enabled_{0};
};

class SPI16DBus : public SPIBus {
  void dump_config() override;

 protected:
  void data(const uint8_t *value, size_t len) override;
};

#ifdef USE_ESP32_VARIANT_ESP32S3
class RGBBus : public SPIBus {
  void setup(uint16_t width, uint16_t height) override;
  void dump_config() override;

  void begin_pixels() override {}
  void end_pixels() override {}
  void send_pixels(Rect window, const uint8_t *data, size_t len);

  void add_data_pin(InternalGPIOPin *data_pin, size_t index) { this->data_pins_[index] = data_pin; };
  void set_de_pin(InternalGPIOPin *de_pin) { this->de_pin_ = de_pin; }

  void set_pclk_pin(InternalGPIOPin *pclk_pin) { this->pclk_pin_ = pclk_pin; }
  void set_pclk_frequency(uint32_t pclk_frequency) { this->pclk_frequency_ = pclk_frequency; }
  void set_pclk_inverted(bool inverted) { this->pclk_inverted_ = inverted; }

  void set_vsync_pin(InternalGPIOPin *vsync_pin) { this->vsync_pin_ = vsync_pin; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }

  void set_hsync_pin(InternalGPIOPin *hsync_pin) { this->hsync_pin_ = hsync_pin; }
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }

 protected:
  void data(const uint8_t *value, size_t len) override{};
  void send_byte(uint8_t data) override{};
  void send_array(const uint8_t *data, size_t len) override{};

  InternalGPIOPin *data_pins_[16] = {};
  InternalGPIOPin *de_pin_{nullptr};

  InternalGPIOPin *pclk_pin_{nullptr};
  uint32_t pclk_frequency_ = 16 * 1000 * 1000;
  bool pclk_inverted_{true};

  InternalGPIOPin *hsync_pin_{nullptr};
  uint16_t hsync_front_porch_ = 8;
  uint16_t hsync_pulse_width_ = 4;
  uint16_t hsync_back_porch_ = 8;

  InternalGPIOPin *vsync_pin_{nullptr};
  uint16_t vsync_front_porch_ = 8;
  uint16_t vsync_pulse_width_ = 4;
  uint16_t vsync_back_porch_ = 8;

  esp_lcd_panel_handle_t handle_{};
};
#endif

}  // namespace display
}  // namespace esphome

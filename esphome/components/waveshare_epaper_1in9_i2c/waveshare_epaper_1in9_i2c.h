#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

#include "display_constants.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

class WaveShareEPaper1in9I2C : public PollingComponent {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::IO; };

  void create_command_device(i2c::I2CBus *bus, uint8_t address) {
    this->command_device_ = new i2c::I2CDevice();
    this->command_device_->set_i2c_address(address);
    this->command_device_->set_i2c_bus(bus);
    this->command_device_address_ = address;
  }

  void create_data_device(i2c::I2CBus *bus, uint8_t address) {
    this->data_device_ = new i2c::I2CDevice();
    this->data_device_->set_i2c_address(address);
    this->data_device_->set_i2c_bus(bus);
    this->data_device_address_ = address;
  }

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }
  void set_temperature_for_compensation(float temp) { this->compensation_temp_ = temp; }
  void set_temperature(float temp);
  void set_humidity(float humidity);
  void set_low_power_indicator(bool enabled);
  void set_bluetooth_indicator(bool enabled);
  void apply_temperature_compensation();
  void dump_config() override;

 protected:
  float compensation_temp_ = 20;
  bool potential_refresh_ = false;
  bool inverted_colors_ = false;

  uint8_t framebuffer_[FRAMEBUFFER_SIZE] = {CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY,
                                            CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY,
                                            CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY};

  uint8_t degrees_type_ = CHAR_CELSIUS;
  int temperature_digits_[TEMPERATURE_DIGITS_LEN] = {-1, -1, -1, -1};
  bool temperature_positive_ = true;

  int humidity_digits_[HUMIDITY_DIGITS_LEN] = {-1, -1, -1};
  bool humidity_positive_ = true;

  uint8_t command_device_address_;
  i2c::I2CDevice *command_device_;

  uint8_t data_device_address_;
  i2c::I2CDevice *data_device_;
  GPIOPin *reset_pin_;
  GPIOPin *busy_pin_;

  void init_screen_();
  void reset_screen_();
  void read_busy_();
  void write_lut_(const uint8_t lut[LUT_SIZE]);
  void write_screen_();
  void deep_sleep_();
  bool update_framebuffer_(uint8_t new_image[FRAMEBUFFER_SIZE]);

  void send_commands_(const uint8_t *data, uint8_t len, bool stop = true);
  void send_data_(const uint8_t *data, uint8_t len, bool stop = true);
  void send_reset_(bool value) { this->reset_pin_->digital_write(value); };
};
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

#include "display_constants.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

template<typename T> struct ValueState {
  T current;
  T future;

  bool is_changed() { return this->current != this->future; }
  void flip() { this->current = this->future; }
};

struct DisplayState {
  ValueState<int16_t> temperature_x10{TEMPERATURE_X10_MAX + 1, TEMPERATURE_X10_MAX + 1};
  ValueState<int16_t> humidity_x10{HUMIDITY_X10_MAX + 1, HUMIDITY_X10_MAX + 1};
  ValueState<bool> low_power{false, false};
  ValueState<bool> bluetooth{false, false};
  ValueState<bool> is_celsius{true, true};
  ValueState<bool> display_percent{true, true};
  ValueState<bool> display_temperature_unit{true, true};

  bool is_changed() {
    return this->temperature_x10.is_changed() || this->humidity_x10.is_changed() || this->low_power.is_changed() ||
           this->bluetooth.is_changed() || this->is_celsius.is_changed() || this->display_percent.is_changed() ||
           this->display_temperature_unit.is_changed();
  }
  void flip() {
    this->temperature_x10.flip();
    this->humidity_x10.flip();
    this->low_power.flip();
    this->bluetooth.flip();
    this->is_celsius.flip();
    this->display_percent.flip();
    this->display_temperature_unit.flip();
  }
};

class WaveShareEPaper1in9I2C;

using waveshare_epaper_1in9_i2c_writer_t = std::function<void(WaveShareEPaper1in9I2C &)>;

class WaveShareEPaper1in9I2C : public PollingComponent {
 public:
  void set_writer(waveshare_epaper_1in9_i2c_writer_t &&writer) { this->writer_ = writer; }

  void setup() override;
  void update() override;
  void display();

  float get_setup_priority() const override { return setup_priority::PROCESSOR; };

  void create_command_device(i2c::I2CBus *bus, uint8_t address) {
    this->command_device_.set_i2c_address(address);
    this->command_device_.set_i2c_bus(bus);
    this->command_device_address_ = address;
  }

  void create_data_device(i2c::I2CBus *bus, uint8_t address) {
    this->data_device_.set_i2c_address(address);
    this->data_device_.set_i2c_bus(bus);
    this->data_device_address_ = address;
  }

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }
  void set_full_update_every(uint32_t full_update_every) { this->full_update_every_ = full_update_every; }

  void set_temperature_for_compensation(float temp) { this->compensation_temp_ = temp; }
  void set_temperature(float temp);
  void set_temperature_unit(const char *unit);

  void set_humidity(float humidity);

  void display_percent(bool display_percent);
  void display_temperature_unit(bool display_temperature_unit);

  void display_low_power_indicator(bool is_low_power);
  void display_bluetooth_indicator(bool is_bluetooth);

  void apply_temperature_compensation();
  void dump_config() override;

 protected:
  float compensation_temp_{20};
  bool inverted_colors_{false};

  DisplayState display_state_;

  uint32_t full_update_every_{30};
  uint32_t at_update_{0};

  uint8_t command_device_address_;
  i2c::I2CDevice command_device_;

  uint8_t data_device_address_;
  i2c::I2CDevice data_device_;
  GPIOPin *reset_pin_;
  GPIOPin *busy_pin_;
  optional<waveshare_epaper_1in9_i2c_writer_t> writer_{};

  void init_screen_();
  void reset_screen_();
  void wait_for_idle_();
  void write_lut_(const uint8_t lut[LUT_SIZE]);
  void write_screen_(const uint8_t framebuffer[FRAMEBUFFER_SIZE]);
  void deep_sleep_();

  void send_commands_(const uint8_t *data, uint8_t len, bool stop = true);
  void send_data_(const uint8_t *data, uint8_t len, bool stop = true);
  void send_reset_(bool value) { this->reset_pin_->digital_write(value); };
};
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome

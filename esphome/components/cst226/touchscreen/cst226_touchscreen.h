#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst226 {

static const char *const TAG = "cst226.touchscreen";

static const uint8_t CST226_REG_STATUS = 0x00;

class CST226ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST226Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override {
    esph_log_config(TAG, "Setting up CST226 Touchscreen...");
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->setup();
      this->reset_pin_->digital_write(true);
      delay(5);
      this->reset_pin_->digital_write(false);
      delay(5);
      this->reset_pin_->digital_write(true);
      this->set_timeout(30, [this] { this->continue_setup_(); });
    } else {
      this->continue_setup_();
    }
  }

  void update_touches() override {
    uint8_t data[28];
    if (!this->read_bytes(CST226_REG_STATUS, data, sizeof data)) {
      this->status_set_warning();
      this->skip_update_ = true;
      return;
    }
    if (data[6] != 0xAB || data[0] == 0xAB || data[5] == 0x80) {
      this->skip_update_ = true;
      esph_log_d(TAG, "data[6] = %X, data[0] == %X, data[5] == %X", data[6], data[0], data[5]);
      return;
    }
    uint8_t num_of_touches = data[5] & 0x7F;
    if (num_of_touches == 0 || num_of_touches > 5) {
      this->write_byte(0, 0xAB);
      return;
    }

    size_t index = 0;
    for (uint8_t i = 0; i != num_of_touches; i++) {
      uint8_t id = data[index] >> 4;
      int16_t x = (data[index + 1] << 4) | ((data[index + 3] >> 4) & 0x0F);
      int16_t y = (data[index + 2] << 4) | (data[index + 3] & 0x0F);
      int16_t z = data[index + 4];
      this->add_raw_touch_position_(id, x, y, z);
      esph_log_v(TAG, "Read touch %d: %d/%d", id, x, y);
      index += 5;
      if (i == 0)
        index += 2;
    }
  }

  void register_button_listener(CST226ButtonListener *listener) { this->button_listeners_.push_back(listener); }
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  bool read16(uint16_t addr, uint8_t * data, size_t len) {
    if (this->read_register16(addr, data, len) != i2c::ERROR_OK) {
      esph_log_e(TAG, "Read data from 0x%04X failed", addr);
      this->mark_failed();
      return false;
    }
    return true;
  }
  void continue_setup_() {
    uint8_t buffer[8];
    if (this->interrupt_pin_ != nullptr) {
      this->interrupt_pin_->setup();
      this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
    }
    buffer[0] = 0xD1;
    if (this->write_register16(0xD1, buffer, 1) != i2c::ERROR_OK) {
      esph_log_e(TAG, "Write byte to 0xD1 failed");
      this->mark_failed();
      return;
    }
    delay(10);
    if (!this->read16(0xD204, buffer, 4))
      return;
    uint16_t chip_id = buffer[2] + (buffer[3] << 8);
    uint16_t project_id = buffer[0] + (buffer[1] << 8);
    esph_log_config(TAG, "Chip ID %X, project ID %x", chip_id, project_id);
    if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
      if (!this->read16(0xD1F8, buffer, 4))
        return;
      this->x_raw_max_ = buffer[0] + (buffer[1] << 8);
      this->y_raw_max_ = buffer[2] + (buffer[3] << 8);
    }
    esph_log_config(TAG, "CST226 Touchscreen setup complete");
  }

  void update_button_state_(bool state) {
    if (this->button_touched_ == state)
      return;
    this->button_touched_ = state;
    for (auto *listener : this->button_listeners_)
      listener->update_button(state);
  }

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint8_t chip_id_{};
  std::vector<CST226ButtonListener *> button_listeners_;
  bool button_touched_{};
};

}  // namespace cst226
}  // namespace esphome

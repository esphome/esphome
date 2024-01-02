#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst816 {

static const char *const TAG = "cst816.touchscreen";

static const uint8_t CST816_REG_STATUS = 0x00;
static const uint8_t CST816_REG_TOUCH_NUM = 0x02;
static const uint8_t CST816_REG_XPOS_HIGH = 0x03;
static const uint8_t CST816_REG_XPOS_LOW = 0x04;
static const uint8_t CST816_REG_YPOS_HIGH = 0x05;
static const uint8_t CST816_REG_YPOS_LOW = 0x06;
static const uint8_t CST816_REG_DIS_AUTOSLEEP = 0xFE;
static const uint8_t CST816_REG_CHIP_ID = 0xA7;
static const uint8_t CST816_REG_FW_VERSION = 0xA9;
static const uint8_t CST816_REG_SLEEP = 0xE5;
static const uint8_t CST816_REG_IRQ_CTL = 0xFA;
static const uint8_t CST816_IRQ_EN_MOTION = 0x70;

static const uint8_t CST816S_CHIP_ID = 0xB4;
static const uint8_t CST816D_CHIP_ID = 0xB6;
static const uint8_t CST816T_CHIP_ID = 0xB5;
static const uint8_t CST716_CHIP_ID = 0x20;

class CST816ButtonListener {
 public:
  virtual void update_button(bool state) = 0;
};

class CST816Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override {
    esph_log_config(TAG, "Setting up CST816 Touchscreen...");
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
    uint8_t data[13];
    if (!this->read_bytes(CST816_REG_STATUS, data, sizeof data)) {
      this->status_set_warning();
      return;
    }
    uint8_t num_of_touches = data[CST816_REG_TOUCH_NUM] & 3;
    if (num_of_touches == 0) {
      this->update_button_state_(false);
      return;
    }

    uint16_t x = encode_uint16(data[CST816_REG_XPOS_HIGH] & 0xF, data[CST816_REG_XPOS_LOW]);
    uint16_t y = encode_uint16(data[CST816_REG_YPOS_HIGH] & 0xF, data[CST816_REG_YPOS_LOW]);
    esph_log_v(TAG, "Read touch %d/%d", x, y);
    if (x > this->x_raw_max_) {
      this->update_button_state_(true);
    } else {
      this->add_raw_touch_position_(0, x, y);
    }
  }

  void register_button_listener(CST816ButtonListener *listener) { this->button_listeners_.push_back(listener); }
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void continue_setup_() {
    if (this->interrupt_pin_ != nullptr) {
      this->interrupt_pin_->setup();
      this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
    }
    if (!this->read_byte(CST816_REG_CHIP_ID, &this->chip_id_)) {
      this->mark_failed();
      esph_log_e(TAG, "Failed to read chip id");
      return;
    }
    switch (this->chip_id_) {
      case CST716_CHIP_ID:
      case CST816S_CHIP_ID:
      case CST816D_CHIP_ID:
      case CST816T_CHIP_ID:
        break;
      default:
        this->mark_failed();
        esph_log_e(TAG, "Unknown chip ID 0x%02X", this->chip_id_);
        return;
    }
    this->write_byte(CST816_REG_IRQ_CTL, CST816_IRQ_EN_MOTION);
    this->x_raw_max_ = this->display_->get_width();
    this->y_raw_max_ = this->display_->get_height();
    esph_log_config(TAG, "CST816 Touchscreen setup complete");
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
  std::vector<CST816ButtonListener *> button_listeners_;
  bool button_touched_{};
};

}  // namespace cst816
}  // namespace esphome

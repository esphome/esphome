#include "xl9535.h"
#include "esphome/core/log.h"

namespace esphome {
namespace xl9535 {

static const char *const TAG = "xl9535";

void XL9535Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up XL9535...");

  // Check to see if the device can read from the register
  uint8_t port = 0;
  if (this->read_register(XL9535_INPUT_PORT_0_REGISTER, &port, 1) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
}

void XL9535Component::dump_config() {
  ESP_LOGCONFIG(TAG, "XL9535:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with XL9535 failed!");
  }
}

bool XL9535Component::digital_read(uint8_t pin) {
  bool state = false;
  uint8_t port = 0;

  if (pin > 7) {
    if (this->read_register(XL9535_INPUT_PORT_1_REGISTER, &port, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return state;
    }

    state = (port & (pin - 10)) != 0;
  } else {
    if (this->read_register(XL9535_INPUT_PORT_0_REGISTER, &port, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return state;
    }

    state = (port & pin) != 0;
  }

  this->status_clear_warning();
  return state;
}

void XL9535Component::digital_write(uint8_t pin, bool value) {
  uint8_t port = 0;
  uint8_t register_data = 0;

  if (pin > 7) {
    if (this->read_register(XL9535_OUTPUT_PORT_1_REGISTER, &register_data, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }

    register_data = register_data & (~(1 << (pin - 10)));
    port = register_data | value << (pin - 10);

    if (this->write_register(XL9535_OUTPUT_PORT_1_REGISTER, &port, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
  } else {
    if (this->read_register(XL9535_OUTPUT_PORT_0_REGISTER, &register_data, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    register_data = register_data & (~(1 << pin));
    port = register_data | value << pin;

    if (this->write_register(XL9535_OUTPUT_PORT_0_REGISTER, &port, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
  }

  this->status_clear_warning();
}

void XL9535Component::pin_mode(uint8_t pin, gpio::Flags mode) {
  uint8_t port = 0;

  if (pin > 7) {
    this->read_register(XL9535_CONFIG_PORT_1_REGISTER, &port, 1);

    if (mode == gpio::FLAG_INPUT) {
      port = port | (1 << (pin - 10));
    } else if (mode == gpio::FLAG_OUTPUT) {
      port = port & (~(1 << (pin - 10)));
    }

    this->write_register(XL9535_CONFIG_PORT_1_REGISTER, &port, 1);
  } else {
    this->read_register(XL9535_CONFIG_PORT_0_REGISTER, &port, 1);

    if (mode == gpio::FLAG_INPUT) {
      port = port | (1 << pin);
    } else if (mode == gpio::FLAG_OUTPUT) {
      port = port & (~(1 << pin));
    }

    this->write_register(XL9535_CONFIG_PORT_0_REGISTER, &port, 1);
  }
}

void XL9535GPIOPin::setup() { this->pin_mode(this->flags_); }

std::string XL9535GPIOPin::dump_summary() const { return str_snprintf("%u via XL9535", 15, this->pin_); }

void XL9535GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool XL9535GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void XL9535GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace xl9535
}  // namespace esphome

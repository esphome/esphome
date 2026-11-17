#include "xl9535.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::xl9535 {

ESPHOME_LOG_TAG(TAG, "xl9535");

// Start from the levels the chip already holds, which survive a reboot of the MCU
bool XL9535Component::load_outputs_() {
  this->outputs_loaded_ = this->read_byte(XL9535_OUTPUT_PORT_0_REGISTER, &this->output_[0]) &&
                          this->read_byte(XL9535_OUTPUT_PORT_1_REGISTER, &this->output_[1]);
  return this->outputs_loaded_;
}

void XL9535Component::setup() { this->setup_check(this->outputs_loaded_ || this->load_outputs_()); }

void XL9535Component::dump_config() {
  ESP_LOGCONFIG(TAG, "XL9535:");
  LOG_I2C_DEVICE(this);
}

// Pins 0-7 are port 0 and pins 10-17 are port 1; each register pair holds port 0 then port 1
static uint8_t port_index(uint8_t pin) { return pin / 10; }
static uint8_t port_register(uint8_t port_0_register, uint8_t pin) { return port_0_register + port_index(pin); }
static uint8_t pin_mask(uint8_t pin) { return 1 << (pin % 10); }

bool XL9535Component::digital_read(uint8_t pin) {
  uint8_t port = 0;
  if (!this->status_comm_result(this->read_byte(port_register(XL9535_INPUT_PORT_0_REGISTER, pin), &port)))
    return false;
  return (port & pin_mask(pin)) != 0;
}

void XL9535Component::digital_write(uint8_t pin, bool value) {
  // Without the chip's levels a write would clear the port's other outputs
  if (this->is_failed() || (!this->outputs_loaded_ && !this->status_comm_result(this->load_outputs_())))
    return;
  uint8_t &output = this->output_[port_index(pin)];
  assign_bits(output, pin_mask(pin), value);
  this->status_comm_result(this->write_byte(port_register(XL9535_OUTPUT_PORT_0_REGISTER, pin), output));
}

void XL9535Component::pin_mode(uint8_t pin, gpio::Flags mode) {
  if (mode != gpio::FLAG_INPUT && mode != gpio::FLAG_OUTPUT)
    return;
  // A set config bit makes the pin an input
  const uint8_t mask = pin_mask(pin);
  this->status_comm_result(this->update_register(port_register(XL9535_CONFIG_PORT_0_REGISTER, pin), mask,
                                                 mode == gpio::FLAG_INPUT ? mask : 0));
}

}  // namespace esphome::xl9535

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::xl9535::XL9535Component, "%u via XL9535")

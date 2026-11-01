#include "tca9555.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const uint8_t TCA9555_INPUT_PORT_REGISTER_0 = 0x00;
static const uint8_t TCA9555_INPUT_PORT_REGISTER_1 = 0x01;
static const uint8_t TCA9555_OUTPUT_PORT_REGISTER_0 = 0x02;
static const uint8_t TCA9555_OUTPUT_PORT_REGISTER_1 = 0x03;
static const uint8_t TCA9555_POLARITY_REGISTER_0 = 0x04;
static const uint8_t TCA9555_POLARITY_REGISTER_1 = 0x05;
static const uint8_t TCA9555_CONFIGURATION_PORT_0 = 0x06;
static const uint8_t TCA9555_CONFIGURATION_PORT_1 = 0x07;

namespace esphome::tca9555 {

ESPHOME_LOG_TAG(TAG, "tca9555");

void TCA9555Component::setup() {
  if (!this->read_gpio_modes_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (!this->read_gpio_outputs_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  this->setup_interrupt_pin_();
}
void TCA9555Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TCA9555:");
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
}
void TCA9555Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    // Set mode mask bit
    this->mode_mask_ |= 1 << pin;
    this->enable_polling_loop_();
  } else if (flags == gpio::FLAG_OUTPUT) {
    // Clear mode mask bit
    this->mode_mask_ &= ~(1 << pin);
  }
  // Write GPIO to enable input mode
  this->write_gpio_modes_();
}

bool TCA9555Component::read_gpio_outputs_() {
  if (this->is_failed())
    return false;
  uint16_t value;
  if (!this->status_comm_result(this->read_byte_16_le(TCA9555_OUTPUT_PORT_REGISTER_0, &value),
                                LOG_STR("Failed to read output register")))
    return false;
  this->output_mask_ = value;
  return true;
}

bool TCA9555Component::read_gpio_modes_() {
  if (this->is_failed())
    return false;
  uint16_t value;
  if (!this->status_comm_result(this->read_byte_16_le(TCA9555_CONFIGURATION_PORT_0, &value),
                                LOG_STR("Failed to read mode register")))
    return false;
  this->mode_mask_ = value;
  return true;
}
bool TCA9555Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;
  uint8_t data;
  if (!this->status_comm_result(
          this->read_byte(pin < 8 ? TCA9555_INPUT_PORT_REGISTER_0 : TCA9555_INPUT_PORT_REGISTER_1, &data),
          LOG_STR("Failed to read input register")))
    return false;
  this->input_[pin / 8] = data;
  return true;
}

void TCA9555Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  assign_bits(this->output_mask_, 1 << pin, value);

  this->status_comm_result(this->write_byte_16_le(TCA9555_OUTPUT_PORT_REGISTER_0, this->output_mask_),
                           LOG_STR("Failed to write output register"));
}

bool TCA9555Component::write_gpio_modes_() {
  if (this->is_failed())
    return false;
  return this->status_comm_result(this->write_byte_16_le(TCA9555_CONFIGURATION_PORT_0, this->mode_mask_),
                                  LOG_STR("Failed to write mode register"));
}

}  // namespace esphome::tca9555

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::tca9555::TCA9555Component, "%u via TCA9555")

#include "x9c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace x9c {

static const char *const TAG = "x9c.output";

void X9cOutput::trim_value(int change_amount) {
  if (change_amount == 0) {
    return;
  }

  if (change_amount > 0) {  // Set change direction
    this->ud_pin_->digital_write(true);
  } else {
    this->ud_pin_->digital_write(false);
  }

  this->inc_pin_->digital_write(true);
  this->cs_pin_->digital_write(false);  // Select chip

  for (int i = 0; i < abs(change_amount); i++) {  // Move wiper
    this->inc_pin_->digital_write(true);
    delayMicroseconds(1);
    this->inc_pin_->digital_write(false);
    delayMicroseconds(1);
  }

  delayMicroseconds(100);  // Let value settle

  this->inc_pin_->digital_write(false);
  this->cs_pin_->digital_write(true);  // Deselect chip safely (no save)
}

void X9cOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up X9C Potentiometer with initial value of %f", this->initial_value_);

  this->inc_pin_->get_pin();
  this->inc_pin_->setup();
  this->inc_pin_->digital_write(false);

  this->cs_pin_->get_pin();
  this->cs_pin_->setup();
  this->cs_pin_->digital_write(true);

  this->ud_pin_->get_pin();
  this->ud_pin_->setup();

  if (this->initial_value_ <= 0.50) {
    this->trim_value(-101);  // Set min value (beyond 0)
    this->trim_value(static_cast<uint32_t>(roundf(this->initial_value_ * 100)));
  } else {
    this->trim_value(101);  // Set max value (beyond 100)
    this->trim_value(static_cast<uint32_t>(roundf(this->initial_value_ * 100) - 100));
  }
  this->pot_value_ = this->initial_value_;
  this->write_state(this->initial_value_);
}

void X9cOutput::write_state(float state) {
  this->trim_value(static_cast<uint32_t>(roundf((state - this->pot_value_) * 100)));
  this->pot_value_ = state;
}

void X9cOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "X9C Potentiometer Output:");
  LOG_PIN("  Chip Select Pin: ", this->cs_pin_);
  LOG_PIN("  Increment Pin: ", this->inc_pin_);
  LOG_PIN("  Up/Down Pin: ", this->ud_pin_);
  ESP_LOGCONFIG(TAG, "  Initial Value: %f", this->initial_value_);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace x9c
}  // namespace esphome

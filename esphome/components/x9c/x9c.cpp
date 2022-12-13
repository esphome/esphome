#include "x9c.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace x9c {

static const char *const TAG = "x9c.number";

void X9cNumber::trim_value(int change_amount) {
  if (change_amount > 0) {                             // Set change direction
    this->ud_pin_->digital_write(true);
  } else {
    this->ud_pin_->digital_write(false);
  }

  this->inc_pin_->digital_write(true);    
  this->cs_pin_->digital_write(false);                 // Select chip

  for (int i = 0; i < abs(change_amount); i++) {       // Move wiper
    this->inc_pin_->digital_write(true);
    delayMicroseconds(1);
    this->inc_pin_->digital_write(false);
    delayMicroseconds(1);
    }

  delayMicroseconds(100);                              // Let value settle

  this->inc_pin_->digital_write(false);    
  this->cs_pin_->digital_write(true);                   // Deselect chip safely (no save)
}

void X9cNumber::setup() {
  ESP_LOGCONFIG(TAG, "Setting up X9C Potentiometer with initial value of %i", this->initial_value_);

  uint8_t inc = inc_pin_->get_pin();
  this->inc_pin_->setup();
  this->inc_pin_->digital_write(false);

  uint8_t cs = cs_pin_->get_pin();
  this->cs_pin_->setup();
  this->cs_pin_->digital_write(true);
  
  uint8_t ud = ud_pin_->get_pin();
  this->ud_pin_->setup();

  if (this->initial_value_ < 51) {
    this->trim_value(-101);                            // Set min value (beyond 0)
    this->trim_value(this->initial_value_);
  } else {
    this->trim_value(101);                             // Set max value (beyond 100)
    this->trim_value(this->initial_value_ - 100);
  }
  this->publish_state((float)this->initial_value_);
}

void X9cNumber::control(float value) {
  this->trim_value((int)(value - this->state));
  this->publish_state(value);
}

void X9cNumber::dump_config() {
  LOG_NUMBER("", "X9c Potentiometer Number", this);
  LOG_PIN("  Chip Select Pin: ", this->cs_pin_);
  LOG_PIN("  Increment Pin: ", this->inc_pin_);
  LOG_PIN("  Up/Down Pin: ", this->ud_pin_);
  ESP_LOGCONFIG(TAG, "  Initial Value: %i", this->initial_value_);
}

}  // namespace x9c
}  // namespace esphome

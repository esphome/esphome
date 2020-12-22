#include "rotary_encoder.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace rotary_encoder {

static const char *TAG = "rotary_encoder";

// based on https://github.com/jkDesignDE/MechInputs/blob/master/QEIx4.cpp
static const uint8_t STATE_LUT_MASK = 0x1C;  // clears upper counter increment/decrement bits and pin states
static const uint16_t STATE_PIN_A_HIGH = 0x01;
static const uint16_t STATE_PIN_B_HIGH = 0x02;
static const uint16_t STATE_S0 = 0x00;
static const uint16_t STATE_S1 = 0x04;
static const uint16_t STATE_S2 = 0x08;
static const uint16_t STATE_S3 = 0x0C;
static const uint16_t STATE_CCW = 0x00;
static const uint16_t STATE_CW = 0x10;
static const uint16_t STATE_HAS_INCREMENTED = 0x0700;
static const uint16_t STATE_INCREMENT_COUNTER_4 = 0x0700;
static const uint16_t STATE_INCREMENT_COUNTER_2 = 0x0300;
static const uint16_t STATE_INCREMENT_COUNTER_1 = 0x0100;
static const uint16_t STATE_HAS_DECREMENTED = 0x7000;
static const uint16_t STATE_DECREMENT_COUNTER_4 = 0x7000;
static const uint16_t STATE_DECREMENT_COUNTER_2 = 0x3000;
static const uint16_t STATE_DECREMENT_COUNTER_1 = 0x1000;

// State explanation: 8-bit uint
// Bit 0 (0x01) encodes Pin A HIGH/LOW (reset before each read)
// Bit 1 (0x02) encodes Pin B HIGH/LOW (reset before each read)
// Bit 2&3 (0x0C) encodes state S0-S3
// Bit 4 (0x10) encodes clockwise/counter-clockwise rotation

// Only apply if DRAM_ATTR exists on this platform (exists on ESP32, not on ESP8266)
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif
// array needs to be placed in .dram1 for ESP32
// otherwise it will automatically go into flash, and cause cache disabled issues
static const uint16_t DRAM_ATTR STATE_LOOKUP_TABLE[32] = {
    // act state S0 in CCW direction
    STATE_CCW | STATE_S0,                              // 0x00: stay here
    STATE_CW | STATE_S1 | STATE_INCREMENT_COUNTER_1,   // 0x01: goto CW+S1 and increment counter (dir change)
    STATE_CCW | STATE_S0,                              // 0x02: stay here
    STATE_CCW | STATE_S3 | STATE_DECREMENT_COUNTER_4,  // 0x03: goto CCW+S3 and decrement counter
    // act state S1 in CCW direction
    STATE_CCW | STATE_S1,                              // 0x04: stay here
    STATE_CCW | STATE_S1,                              // 0x05: stay here
    STATE_CCW | STATE_S0 | STATE_DECREMENT_COUNTER_1,  // 0x06: goto CCW+S0 and decrement counter
    STATE_CW | STATE_S2 | STATE_INCREMENT_COUNTER_4,   // 0x07: goto CW+S2 and increment counter (dir change)
    // act state S2 in CCW direction
    STATE_CCW | STATE_S1 | STATE_DECREMENT_COUNTER_2,  // 0x08: goto CCW+S1 and decrement counter
    STATE_CCW | STATE_S2,                              // 0x09: stay here
    STATE_CW | STATE_S3 | STATE_INCREMENT_COUNTER_1,   // 0x0A: goto CW+S3 and increment counter (dir change)
    STATE_CCW | STATE_S2,                              // 0x0B: stay here
    // act state S3 in CCW direction
    STATE_CW | STATE_S0 | STATE_INCREMENT_COUNTER_2,   // 0x0C: goto CW+S0 and increment counter (dir change)
    STATE_CCW | STATE_S2 | STATE_DECREMENT_COUNTER_1,  // 0x0D: goto CCW+S2 and decrement counter
    STATE_CCW | STATE_S3,                              // 0x0E: stay here
    STATE_CCW | STATE_S3,                              // 0x0F: stay here

    // act state S0 in CW direction
    STATE_CW | STATE_S0,                               // 0x10: stay here
    STATE_CW | STATE_S1 | STATE_INCREMENT_COUNTER_1,   // 0x11: goto CW+S1 and increment counter
    STATE_CW | STATE_S0,                               // 0x12: stay here
    STATE_CCW | STATE_S3 | STATE_DECREMENT_COUNTER_4,  // 0x13: goto CCW+S3 and decrement counter (dir change)
    // act state S1 in CW direction
    STATE_CW | STATE_S1,                               // 0x14: stay here
    STATE_CW | STATE_S1,                               // 0x15: stay here
    STATE_CCW | STATE_S0 | STATE_DECREMENT_COUNTER_1,  // 0x16: goto CCW+S0 and decrement counter (dir change)
    STATE_CW | STATE_S2 | STATE_INCREMENT_COUNTER_4,   // 0x17: goto CW+S2 and increment counter
    // act state S2 in CW direction
    STATE_CCW | STATE_S1 | STATE_DECREMENT_COUNTER_2,  // 0x18: goto CCW+S1 and decrement counter (dir change)
    STATE_CW | STATE_S2,                               // 0x19: stay here
    STATE_CW | STATE_S3 | STATE_INCREMENT_COUNTER_1,   // 0x1A: goto CW+S3 and increment counter
    STATE_CW | STATE_S2,
    // act state S3 in CW direction
    STATE_CW | STATE_S0 | STATE_INCREMENT_COUNTER_2,   // 0x1C: goto CW+S0 and increment counter
    STATE_CCW | STATE_S2 | STATE_DECREMENT_COUNTER_1,  // 0x1D: goto CCW+S2 and decrement counter (dir change)
    STATE_CW | STATE_S3,                               // 0x1E: stay here
    STATE_CW | STATE_S3                                // 0x1F: stay here
};

void ICACHE_RAM_ATTR HOT RotaryEncoderSensorStore::gpio_intr(RotaryEncoderSensorStore *arg) {
  // Forget upper bits and add pin states
  uint8_t input_state = arg->state & STATE_LUT_MASK;
  if (arg->pin_a->digital_read())
    input_state |= STATE_PIN_A_HIGH;
  if (arg->pin_b->digital_read())
    input_state |= STATE_PIN_B_HIGH;

  uint16_t new_state = STATE_LOOKUP_TABLE[input_state];
  if ((new_state & arg->resolution & STATE_HAS_INCREMENTED) != 0) {
    if (arg->counter < arg->max_value)
      arg->counter++;
    arg->on_clockwise_callback_.call();
  }
  if ((new_state & arg->resolution & STATE_HAS_DECREMENTED) != 0) {
    if (arg->counter > arg->min_value)
      arg->counter--;
    arg->on_anticlockwise_callback_.call();
  }

  arg->state = new_state;
}

void RotaryEncoderSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Rotary Encoder '%s'...", this->name_.c_str());
  this->pin_a_->setup();
  this->store_.pin_a = this->pin_a_->to_isr();
  this->pin_b_->setup();
  this->store_.pin_b = this->pin_b_->to_isr();

  if (this->pin_i_ != nullptr) {
    this->pin_i_->setup();
  }

  this->pin_a_->attach_interrupt(RotaryEncoderSensorStore::gpio_intr, &this->store_, CHANGE);
  this->pin_b_->attach_interrupt(RotaryEncoderSensorStore::gpio_intr, &this->store_, CHANGE);
}
void RotaryEncoderSensor::dump_config() {
  LOG_SENSOR("", "Rotary Encoder", this);
  LOG_PIN("  Pin A: ", this->pin_a_);
  LOG_PIN("  Pin B: ", this->pin_b_);
  LOG_PIN("  Pin I: ", this->pin_i_);
  switch (this->store_.resolution) {
    case ROTARY_ENCODER_1_PULSE_PER_CYCLE:
      ESP_LOGCONFIG(TAG, "  Resolution: 1 Pulse Per Cycle");
      break;
    case ROTARY_ENCODER_2_PULSES_PER_CYCLE:
      ESP_LOGCONFIG(TAG, "  Resolution: 2 Pulses Per Cycle");
      break;
    case ROTARY_ENCODER_4_PULSES_PER_CYCLE:
      ESP_LOGCONFIG(TAG, "  Resolution: 4 Pulse Per Cycle");
      break;
  }
}
void RotaryEncoderSensor::loop() {
  if (this->pin_i_ != nullptr && this->pin_i_->digital_read()) {
    this->store_.counter = 0;
  }
  int counter = this->store_.counter;
  if (this->store_.last_read != counter) {
    this->store_.last_read = counter;
    this->publish_state(counter);
  }
}

float RotaryEncoderSensor::get_setup_priority() const { return setup_priority::DATA; }
void RotaryEncoderSensor::set_resolution(RotaryEncoderResolution mode) { this->store_.resolution = mode; }
void RotaryEncoderSensor::set_min_value(int32_t min_value) { this->store_.min_value = min_value; }
void RotaryEncoderSensor::set_max_value(int32_t max_value) { this->store_.max_value = max_value; }

}  // namespace rotary_encoder
}  // namespace esphome

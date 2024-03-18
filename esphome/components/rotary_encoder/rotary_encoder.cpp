#include "rotary_encoder.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace rotary_encoder {

static const char *const TAG = "rotary_encoder";

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

void IRAM_ATTR HOT RotaryEncoderSensorStore::gpio_intr(RotaryEncoderSensorStore *arg) {
  // Forget upper bits and add pin states
  uint8_t input_state = arg->state & STATE_LUT_MASK;
  if (arg->pin_a.digital_read())
    input_state |= STATE_PIN_A_HIGH;
  if (arg->pin_b.digital_read())
    input_state |= STATE_PIN_B_HIGH;

  int8_t rotation_dir = 0;
  uint16_t new_state = STATE_LOOKUP_TABLE[input_state];
  if ((new_state & arg->resolution & STATE_HAS_INCREMENTED) != 0) {
    if (arg->counter < arg->max_value)
      arg->counter++;
    rotation_dir = 1;
  }
  if ((new_state & arg->resolution & STATE_HAS_DECREMENTED) != 0) {
    if (arg->counter > arg->min_value)
      arg->counter--;
    rotation_dir = -1;
  }

  if (rotation_dir != 0 && !arg->first_read) {
    auto *first_zero = std::find(arg->rotation_events.begin(), arg->rotation_events.end(), 0);  // find first zero
    if (first_zero == arg->rotation_events.begin()  // are we at the start (first event this loop iteration)
        || std::signbit(*std::prev(first_zero)) !=
               std::signbit(rotation_dir)  // or is the last stored event the wrong direction
        || *std::prev(first_zero) == std::numeric_limits<int8_t>::lowest()  // or the last event slot is full (negative)
        || *std::prev(first_zero) == std::numeric_limits<int8_t>::max()) {  // or the last event slot is full (positive)
      if (first_zero != arg->rotation_events.end()) {                       // we have a free rotation slot
        *first_zero += rotation_dir;                                        // store the rotation into a new slot
      } else {
        arg->rotation_events_overflow = true;
      }
    } else {
      *std::prev(first_zero) += rotation_dir;  // store the rotation into the previous slot
    }
  }
  arg->first_read = false;

  arg->state = new_state;
}

void RotaryEncoderSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Rotary Encoder '%s'...", this->name_.c_str());

  int32_t initial_value = 0;
  switch (this->restore_mode_) {
    case ROTARY_ENCODER_RESTORE_DEFAULT_ZERO:
      this->rtc_ = global_preferences->make_preference<int32_t>(this->get_object_id_hash());
      if (!this->rtc_.load(&initial_value)) {
        initial_value = 0;
      }
      break;
    case ROTARY_ENCODER_ALWAYS_ZERO:
      initial_value = 0;
      break;
  }
  initial_value = clamp(initial_value, this->store_.min_value, this->store_.max_value);

  this->store_.counter = initial_value;
  this->store_.last_read = initial_value;

  this->pin_a_->setup();
  this->store_.pin_a = this->pin_a_->to_isr();
  this->pin_b_->setup();
  this->store_.pin_b = this->pin_b_->to_isr();

  if (this->pin_i_ != nullptr) {
    this->pin_i_->setup();
  }

  this->pin_a_->attach_interrupt(RotaryEncoderSensorStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
  this->pin_b_->attach_interrupt(RotaryEncoderSensorStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
}
void RotaryEncoderSensor::dump_config() {
  LOG_SENSOR("", "Rotary Encoder", this);
  LOG_PIN("  Pin A: ", this->pin_a_);
  LOG_PIN("  Pin B: ", this->pin_b_);
  LOG_PIN("  Pin I: ", this->pin_i_);

  const LogString *restore_mode = LOG_STR("");
  switch (this->restore_mode_) {
    case ROTARY_ENCODER_RESTORE_DEFAULT_ZERO:
      restore_mode = LOG_STR("Restore (Defaults to zero)");
      break;
    case ROTARY_ENCODER_ALWAYS_ZERO:
      restore_mode = LOG_STR("Always zero");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Restore Mode: %s", LOG_STR_ARG(restore_mode));

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
  std::array<int8_t, 8> rotation_events;
  bool rotation_events_overflow;
  {
    InterruptLock lock;
    rotation_events = this->store_.rotation_events;
    rotation_events_overflow = this->store_.rotation_events_overflow;

    this->store_.rotation_events.fill(0);
    this->store_.rotation_events_overflow = false;
  }

  if (rotation_events_overflow) {
    ESP_LOGW(TAG, "Captured more rotation events than expected");
  }

  for (auto events : rotation_events) {
    if (events == 0)  // we are at the end of the recorded events
      break;

    if (events > 0) {
      while (events--) {
        this->on_clockwise_callback_.call();
      }
    } else {
      while (events++) {
        this->on_anticlockwise_callback_.call();
      }
    }
  }

  if (this->pin_i_ != nullptr && this->pin_i_->digital_read()) {
    this->store_.counter = 0;
  }
  int counter = this->store_.counter;
  if (this->store_.last_read != counter || this->publish_initial_value_) {
    if (this->restore_mode_ == ROTARY_ENCODER_RESTORE_DEFAULT_ZERO) {
      this->rtc_.save(&counter);
    }
    this->store_.last_read = counter;
    this->publish_state(counter);
    this->listeners_.call(counter);
    this->publish_initial_value_ = false;
  }
}

float RotaryEncoderSensor::get_setup_priority() const { return setup_priority::DATA; }
void RotaryEncoderSensor::set_restore_mode(RotaryEncoderRestoreMode restore_mode) {
  this->restore_mode_ = restore_mode;
}
void RotaryEncoderSensor::set_resolution(RotaryEncoderResolution mode) { this->store_.resolution = mode; }
void RotaryEncoderSensor::set_min_value(int32_t min_value) { this->store_.min_value = min_value; }
void RotaryEncoderSensor::set_max_value(int32_t max_value) { this->store_.max_value = max_value; }

}  // namespace rotary_encoder
}  // namespace esphome

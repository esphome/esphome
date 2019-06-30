#include "as3935.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as3935 {

static const char *TAG = "as3935";

void AS3935Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS3935...");

  this->pin_->setup();
  this->store_.pin = this->pin_->to_isr();

  this->pin_->attach_interrupt(AS3935ComponentStore::gpio_intr, &this->store_, RISING);
}

void AS3935Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS3935:");
  LOG_I2C_DEVICE(this);
}

float AS3935Component::get_setup_priority() const { return setup_priority::DATA; }

void AS3935Component::loop() {
  if (this->store_.interrupt) {
    uint8_t int_value = this->read_interrupt_register_();
    if (int_value == NOISE_INT) {
      ESP_LOGI(TAG, "Noise was detected - try increasing the value!");
    } else if (int_value == DISTURBER_INT) {
      ESP_LOGI(TAG, "Disturber was detected - try increasing the value!");
    } else if (int_value == LIGHTNING_INT) {
      ESP_LOGI(TAG, "Lightning has been detected!");
      if (this->thunder_alert_binary_sensor_ != nullptr)
        this->thunder_alert_binary_sensor_->publish_state(true);
      uint8_t distance = this->get_distance_to_storm_();
      if (this->distance_sensor_ != nullptr)
        this->distance_sensor_->publish_state(distance);
      uint32_t energy = this->get_lightning_energy_();
      if (this->energy_sensor_ != nullptr)
        this->energy_sensor_->publish_state(energy);
    }
    this->thunder_alert_binary_sensor_->publish_state(false);
    this->store_.interrupt = false;
  }
}

// REG0x03, bits [3:0], manufacturer default: 0.
// When there is an event that exceeds the watchdog threshold, the register is written
// with the type of event. This consists of two messages: INT_D (disturber detected) and
// INT_L (Lightning detected). A third interrupt INT_NH (noise level too HIGH)
// indicates that the noise level has been exceeded and will persist until the
// noise has ended. Events are active HIGH. There is a one second window of time to
// read the interrupt register after lightning is detected, and 1.5 after
// disturber.
uint8_t AS3935Component::read_interrupt_register_() {
  // A 2ms delay is added to allow for the memory register to be populated
  // after the interrupt pin goes HIGH. See "Interrupt Management" in
  // datasheet.
  uint8_t interrupt_value;
  if (!this->read_byte(INT_MASK_ANT, &interrupt_value, 2)) {
    ESP_LOGW(TAG, "Read of interrupt register failed!");
    return 0;
  }
  interrupt_value &= (~INT_MASK);  // Only need the first four bits [3:0]
  return interrupt_value;
}

void AS3935Component::set_outdoor_() { this->write_register_(AFE_GAIN, GAIN_MASK, OUTDOOR, 1); }

void AS3935Component::set_indoor_() { this->write_register_(AFE_GAIN, GAIN_MASK, INDOOR, 1); }
// REG0x01, bits[3:0], manufacturer default: 0010 (2).
// This setting determines the threshold for events that trigger the
// IRQ Pin.
void AS3935Component::set_watchdog_threshold_(uint8_t sensitivity) {
  if ((sensitivity < 1) || (sensitivity > 10))  // 10 is the max sensitivity setting
    return;
  this->write_register_(THRESHOLD, THRESH_MASK, sensitivity, 0);
}
// REG0x01, bits [6:4], manufacturer default: 010 (2).
// The noise floor level is compared to a known reference voltage. If this
// level is exceeded the chip will issue an interrupt to the IRQ pin,
// broadcasting that it can not operate properly due to noise (INT_NH).
// Check datasheet for specific noise level tolerances when setting this register.
void AS3935Component::set_noise_level_(uint8_t floor) {
  if ((floor < 1) || (floor > 7))
    return;

  this->write_register_(THRESHOLD, NOISE_FLOOR_MASK, floor, 4);
}

void AS3935Component::set_spike_rejection_(uint8_t spike_sensitivity) {
  if ((spike_sensitivity < 1) || (spike_sensitivity > 11))
    return;

  this->write_register_(LIGHTNING_REG, SPIKE_MASK, spike_sensitivity, 0);
}
// REG0x02, bits [5:4], manufacturer default: 0 (single lightning strike).
// The number of lightning events before IRQ is set high. 15 minutes is The
// window of time before the number of detected lightning events is reset.
// The number of lightning strikes can be set to 1,5,9, or 16.
void AS3935Component::set_lightning_threshold_(uint8_t strikes) {
  if (strikes == 1)
    this->write_register_(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 0, 4);  // Demonstrative
  if (strikes == 5)
    this->write_register_(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 1, 4);
  if (strikes == 9)
    this->write_register_(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 1, 5);
  if (strikes == 16)
    this->write_register_(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 3, 4);
}
// REG0x02, bit [6], manufacturer default: 1.
// This register clears the number of lightning strikes that has been read in
// the last 15 minute block.
void AS3935Component::clear_statistics_() {
  // Write high, then low, then high to clear.
  this->write_register_(LIGHTNING_REG, (1 << 6), 1, 6);
  this->write_register_(LIGHTNING_REG, (1 << 6), 0, 6);
  this->write_register_(LIGHTNING_REG, (1 << 6), 1, 6);
}
// REG0x03, bit [5], manufacturere default: 0.
// This setting will return whether or not disturbers trigger the IRQ Pin.
void AS3935Component::set_mask_disturber_(bool enabled) {
  if (enabled) {
    this->write_register_(INT_MASK_ANT, (1 << 5), 1, 5);
  } else {
    this->write_register_(INT_MASK_ANT, (1 << 5), 0, 5);
  }
}
// REG0x03, bit [7:6], manufacturer default: 0 (16 division ratio).
// The antenna is designed to resonate at 500kHz and so can be tuned with the
// following setting. The accuracy of the antenna must be within 3.5 percent of
// that value for proper signal validation and distance estimation.
void AS3935Component::set_div_ratio_(uint8_t div_ratio) {
  if (div_ratio == 16)
    this->write_register_(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 0, 6);
  else if (div_ratio == 22)
    this->write_register_(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 1, 6);
  else if (div_ratio == 64)
    this->write_register_(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 1, 7);
  else if (div_ratio == 128)
    this->write_register_(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 3, 6);
}
// REG0x07, bit [5:0], manufacturer default: 0.
// This register holds the distance to the front of the storm and not the
// distance to a lightning strike.
uint8_t AS3935Component::get_distance_to_storm_() {
  uint8_t distance;
  if (!this->read_byte(DISTANCE, &distance)) {
    ESP_LOGW(TAG, "Read of distance failed!");
    return 0;
  }
  distance &= (~DISTANCE_MASK);
  return distance;
}
// REG0x08, bits [3:0], manufacturer default: 0.
// This setting will add capacitance to the series RLC antenna on the product
// to help tune its resonance. The datasheet specifies being within 3.5 percent
// of 500kHz to get optimal lightning detection and distance sensing.
// It's possible to add up to 120pF in steps of 8pF to the antenna.
void AS3935Component::set_cap_(uint8_t eight_pico_farad) {
  if (eight_pico_farad > 15)
    return;

  this->write_register_(FREQ_DISP_IRQ, CAP_MASK, eight_pico_farad, 0);
}

uint32_t AS3935Component::get_lightning_energy_() {
  uint32_t pure_light = 0;  // Variable for lightning energy which is just a pure number.
  uint32_t temp = 0;
  uint8_t input;
  // Temp variable for lightning energy.
  if (!this->read_byte(ENERGY_LIGHT_MMSB, &input)) {
    ESP_LOGW(TAG, "Reading of energy failed! (Part 1)");
    return 0;
  }
  temp = input;
  temp &= (~ENERGY_MASK);  // Only interested in the first four bits.
  // Temporary Value is large enough to handle a shift of 16 bits.
  pure_light = temp << 16;
  if (!this->read_byte(ENERGY_LIGHT_MSB, &input)) {
    ESP_LOGW(TAG, "Reading of energy failed! (Part 2)");
    return 0;
  }
  temp = input;
  // Temporary value is large enough to handle a shift of 8 bits.
  pure_light |= temp << 8;
  // No shift here, directly OR'ed into pure_light variable.

  if (!this->read_byte(ENERGY_LIGHT_MSB, &input)) {
    ESP_LOGW(TAG, "Reading of energy failed! (Part 3)");
    return 0;
  }
  temp = input;
  pure_light |= temp;
  return pure_light;
}

void AS3935Component::write_register_(uint8_t reg, uint8_t mask, uint8_t bits, uint8_t start_pos) {
  uint8_t write_reg;
  if (!this->read_byte(reg, &write_reg)) {
    this->mark_failed();
    ESP_LOGW(TAG, "read_byte failed - increase log level for more details!");
    return;
  }

  write_reg &= (~mask);
  write_reg |= (bits << start_pos);

  if (!this->write_byte(reg, write_reg)) {
    ESP_LOGW(TAG, "write_byte failed - increase log level for more details!");
    return;
  }
}

void ICACHE_RAM_ATTR AS3935ComponentStore::gpio_intr(AS3935ComponentStore *arg) { arg->interrupt = true; }

}  // namespace as3935
}  // namespace esphome

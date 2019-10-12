#include "as3935.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as3935 {

static const char *TAG = "as3935";

void AS3935Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS3935...");

  this->pin_->setup();
  this->store_.pin = this->pin_->to_isr();
  LOG_PIN("  Interrupt Pin: ", this->pin_);
  this->pin_->attach_interrupt(AS3935ComponentStore::gpio_intr, &this->store_, RISING);

  // Write properties to sensor
  this->write_indoor(this->indoor_);
  this->write_noise_level(this->noise_level_);
  this->write_watchdog_threshold(this->watchdog_threshold_);
  this->write_spike_rejection(this->spike_rejection_);
  this->write_lightning_threshold(this->lightning_threshold_);
  this->write_mask_disturber(this->mask_disturber_);
  this->write_div_ratio(this->div_ratio_);
  this->write_capacitance(this->capacitance_);
}

void AS3935Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AS3935:");
  LOG_PIN("  Interrupt Pin: ", this->pin_);
}

float AS3935Component::get_setup_priority() const { return setup_priority::DATA; }

void AS3935Component::loop() {
  if (!this->store_.interrupt)
    return;

  uint8_t int_value = this->read_interrupt_register_();
  if (int_value == NOISE_INT) {
    ESP_LOGI(TAG, "Noise was detected - try increasing the noise level value!");
  } else if (int_value == DISTURBER_INT) {
    ESP_LOGI(TAG, "Disturber was detected - try increasing the spike rejection value!");
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

void AS3935Component::write_indoor(bool indoor) {
  ESP_LOGV(TAG, "Setting indoor to %d", indoor);
  if (indoor)
    this->write_register(AFE_GAIN, GAIN_MASK, INDOOR, 1);
  else
    this->write_register(AFE_GAIN, GAIN_MASK, OUTDOOR, 1);
}
// REG0x01, bits[3:0], manufacturer default: 0010 (2).
// This setting determines the threshold for events that trigger the
// IRQ Pin.
void AS3935Component::write_watchdog_threshold(uint8_t watchdog_threshold) {
  ESP_LOGV(TAG, "Setting watchdog sensitivity to %d", watchdog_threshold);
  if ((watchdog_threshold < 1) || (watchdog_threshold > 10))  // 10 is the max sensitivity setting
    return;
  this->write_register(THRESHOLD, THRESH_MASK, watchdog_threshold, 0);
}

// REG0x01, bits [6:4], manufacturer default: 010 (2).
// The noise floor level is compared to a known reference voltage. If this
// level is exceeded the chip will issue an interrupt to the IRQ pin,
// broadcasting that it can not operate properly due to noise (INT_NH).
// Check datasheet for specific noise level tolerances when setting this register.
void AS3935Component::write_noise_level(uint8_t noise_level) {
  ESP_LOGV(TAG, "Setting noise level to %d", noise_level);
  if ((noise_level < 1) || (noise_level > 7))
    return;

  this->write_register(THRESHOLD, NOISE_FLOOR_MASK, noise_level, 4);
}
// REG0x02, bits [3:0], manufacturer default: 0010 (2).
// This setting, like the watchdog threshold, can help determine between false
// events and actual lightning. The shape of the spike is analyzed during the
// chip's signal validation routine. Increasing this value increases robustness
// at the cost of sensitivity to distant events.
void AS3935Component::write_spike_rejection(uint8_t spike_rejection) {
  ESP_LOGV(TAG, "Setting spike rejection to %d", spike_rejection);
  if ((spike_rejection < 1) || (spike_rejection > 11))
    return;

  this->write_register(LIGHTNING_REG, SPIKE_MASK, spike_rejection, 0);
}
// REG0x02, bits [5:4], manufacturer default: 0 (single lightning strike).
// The number of lightning events before IRQ is set high. 15 minutes is The
// window of time before the number of detected lightning events is reset.
// The number of lightning strikes can be set to 1,5,9, or 16.
void AS3935Component::write_lightning_threshold(uint8_t lightning_threshold) {
  ESP_LOGV(TAG, "Setting lightning threshold to %d", lightning_threshold);
  switch (lightning_threshold) {
    case 1:
      this->write_register(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 0, 4);  // Demonstrative
      break;
    case 5:
      this->write_register(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 1, 4);
      break;
    case 9:
      this->write_register(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 1, 5);
      break;
    case 16:
      this->write_register(LIGHTNING_REG, ((1 << 5) | (1 << 4)), 3, 4);
      break;
    default:
      return;
  }
}
// REG0x03, bit [5], manufacturer default: 0.
// This setting will return whether or not disturbers trigger the IRQ Pin.
void AS3935Component::write_mask_disturber(bool enabled) {
  ESP_LOGV(TAG, "Setting mask disturber to %d", enabled);
  if (enabled) {
    this->write_register(INT_MASK_ANT, (1 << 5), 1, 5);
  } else {
    this->write_register(INT_MASK_ANT, (1 << 5), 0, 5);
  }
}
// REG0x03, bit [7:6], manufacturer default: 0 (16 division ratio).
// The antenna is designed to resonate at 500kHz and so can be tuned with the
// following setting. The accuracy of the antenna must be within 3.5 percent of
// that value for proper signal validation and distance estimation.
void AS3935Component::write_div_ratio(uint8_t div_ratio) {
  ESP_LOGV(TAG, "Setting div ratio to %d", div_ratio);
  switch (div_ratio) {
    case 16:
      this->write_register(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 0, 6);
      break;
    case 22:
      this->write_register(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 1, 6);
      break;
    case 64:
      this->write_register(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 1, 7);
      break;
    case 128:
      this->write_register(INT_MASK_ANT, ((1 << 7) | (1 << 6)), 3, 6);
      break;
    default:
      return;
  }
}
// REG0x08, bits [3:0], manufacturer default: 0.
// This setting will add capacitance to the series RLC antenna on the product
// to help tune its resonance. The datasheet specifies being within 3.5 percent
// of 500kHz to get optimal lightning detection and distance sensing.
// It's possible to add up to 120pF in steps of 8pF to the antenna.
void AS3935Component::write_capacitance(uint8_t capacitance) {
  ESP_LOGV(TAG, "Setting tune cap to %d pF", capacitance * 8);
  this->write_register(FREQ_DISP_IRQ, CAP_MASK, capacitance, 0);
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
  ESP_LOGV(TAG, "Calling read_interrupt_register_");
  delay(2);
  return this->read_register_(INT_MASK_ANT, INT_MASK);
}

// REG0x02, bit [6], manufacturer default: 1.
// This register clears the number of lightning strikes that has been read in
// the last 15 minute block.
void AS3935Component::clear_statistics_() {
  // Write high, then low, then high to clear.
  ESP_LOGV(TAG, "Calling clear_statistics_");
  this->write_register(LIGHTNING_REG, (1 << 6), 1, 6);
  this->write_register(LIGHTNING_REG, (1 << 6), 0, 6);
  this->write_register(LIGHTNING_REG, (1 << 6), 1, 6);
}

// REG0x07, bit [5:0], manufacturer default: 0.
// This register holds the distance to the front of the storm and not the
// distance to a lightning strike.
uint8_t AS3935Component::get_distance_to_storm_() {
  ESP_LOGV(TAG, "Calling get_distance_to_storm_");
  return this->read_register_(DISTANCE, DISTANCE_MASK);
}

uint32_t AS3935Component::get_lightning_energy_() {
  ESP_LOGV(TAG, "Calling get_lightning_energy_");
  uint32_t pure_light = 0;  // Variable for lightning energy which is just a pure number.
  uint32_t temp = 0;
  // Temp variable for lightning energy.
  temp = this->read_register_(ENERGY_LIGHT_MMSB, ENERGY_MASK);
  // Temporary Value is large enough to handle a shift of 16 bits.
  pure_light = temp << 16;
  temp = this->read_register(ENERGY_LIGHT_MSB);
  // Temporary value is large enough to handle a shift of 8 bits.
  pure_light |= temp << 8;
  // No shift here, directly OR'ed into pure_light variable.
  temp = this->read_register(ENERGY_LIGHT_LSB);
  pure_light |= temp;
  return pure_light;
}

uint8_t AS3935Component::read_register_(uint8_t reg, uint8_t mask) {
  uint8_t value = this->read_register(reg);
  value &= (~mask);
  return value;
}

void ICACHE_RAM_ATTR AS3935ComponentStore::gpio_intr(AS3935ComponentStore *arg) { arg->interrupt = true; }

}  // namespace as3935
}  // namespace esphome

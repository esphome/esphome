#include "max31855.h"
#ifdef USE_MAX31855_LINEARIZE
#include "max31855_linearize.h"

#include <cmath>
#endif

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::max31855 {

ESPHOME_LOG_TAG(TAG, "max31855");

static constexpr uint32_t READ_TIMEOUT_ID = 0;

void MAX31855Sensor::update() {
  this->enable();
  delay(1);
  // conversion initiated by rising edge
  this->disable();

  // Conversion time typ: 170ms, max: 220ms
  this->set_timeout(READ_TIMEOUT_ID, 220, [this]() { this->read_data_(); });
}

void MAX31855Sensor::setup() { this->spi_setup(); }
void MAX31855Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX31855:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Thermocouple", this);
#ifdef USE_MAX31855_LINEARIZE
  ESP_LOGCONFIG(TAG, "  Type K linearization: %s", YESNO(this->linearize_));
#endif
  if (this->reference_sensor_) {
    LOG_SENSOR("  ", "Reference", this->reference_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Reference temperature disabled.");
  }
}
void MAX31855Sensor::read_data_() {
  this->enable();
  delay(1);
  uint8_t data[4];
  this->read_array(data, 4);
  this->disable();

  const uint32_t mem = encode_uint32(data[0], data[1], data[2], data[3]);

  // Verify we got data
  if (mem != 0xFFFFFFFF) {
    this->status_clear_error();
  } else {
    ESP_LOGE(TAG, "No data received from MAX31855 (0x%08" PRIX32 "). Check wiring!", mem);
    this->publish_state(NAN);
    PUBLISH_STATE(this->reference_sensor_, NAN);
    this->status_set_error();
    return;
  }

  // Internal reference temperature always works
  int16_t ref_val = (mem & 0x0000FFF0) >> 4;
  if (ref_val & 0x0800) {
    ref_val |= 0xF000;  // Pad out 2's complement
  }
  const float t_ref = float(ref_val) * 0.0625f;
  if (this->reference_sensor_) {
    ESP_LOGD(TAG, "Got reference temperature: %.4f°C", t_ref);
    this->reference_sensor_->publish_state(t_ref);
  }

  // Check thermocouple faults
  if (mem & 0x00000001) {
    ESP_LOGW(TAG, "Thermocouple open circuit (not connected) fault from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }
  if (mem & 0x00000002) {
    ESP_LOGW(TAG, "Thermocouple short circuit to ground fault from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }
  if (mem & 0x00000004) {
    ESP_LOGW(TAG, "Thermocouple short circuit to VCC fault from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }
  if (mem & 0x00010000) {
    ESP_LOGW(TAG, "Got faulty reading from MAX31855 (0x%08" PRIX32 ")", mem);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }

  // Decode thermocouple temperature
  int16_t val = (mem & 0xFFFC0000) >> 18;
  if (val & 0x2000) {
    val |= 0xC000;  // Pad out 2's complement
  }
  float t_sense = float(val) * 0.25f;
#ifdef USE_MAX31855_LINEARIZE
  if (this->linearize_)
    t_sense = this->linearize_reading_(t_sense, t_ref);
#endif
  ESP_LOGD(TAG, "Got thermocouple temperature: %.2f°C", t_sense);
  this->publish_state(t_sense);
  this->status_clear_warning();
}

#ifdef USE_MAX31855_LINEARIZE
float MAX31855Sensor::linearize_reading_(float t_chip, float t_ref) {
  const float t_lin = k_type_linearize(t_chip, t_ref);
  if (std::isnan(t_lin)) {
    // Outside the range of the NIST type K tables: keep the chip's own value.
    if (!this->range_warned_) {
      ESP_LOGW(TAG, "Reading %.2f°C is outside the NIST type K range; publishing it uncorrected", t_chip);
      this->range_warned_ = true;
    }
    return t_chip;
  }
  ESP_LOGV(TAG, "Chip reported %.2f°C, type K corrected %.2f°C", t_chip, t_lin);
  return t_lin;
}
#endif

}  // namespace esphome::max31855

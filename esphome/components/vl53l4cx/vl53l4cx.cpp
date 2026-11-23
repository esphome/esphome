#include "vl53l4cx.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::vl53l4cx {

ESPHOME_LOG_TAG(TAG, "vl53l4cx");

static constexpr uint8_t DEFAULT_ADDRESS = 0x29;
// Model id 0xEB and module type 0xAA at IDENTIFICATION__MODEL_ID
static constexpr uint16_t SENSOR_ID = 0xEBAA;

PROGMEM_STRING_TABLE(TargetStrings, "nearest", "farthest", "strongest");
PROGMEM_STRING_TABLE(DistanceModeStrings, "unknown", "short", "medium", "long");

VL53L4CXSensor *VL53L4CXSensor::first_sensor = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

VL53L4CXSensor::VL53L4CXSensor() {
  this->next_ = VL53L4CXSensor::first_sensor;
  VL53L4CXSensor::first_sensor = this;
}

bool VL53L4CXSensor::write_registers(uint8_t address, uint16_t reg, const uint8_t *data, uint16_t len) {
  const uint8_t prefix[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg)};
  return this->bus_->write_prefixed(address, prefix, sizeof(prefix), data, len) == i2c::ERROR_OK;
}

bool VL53L4CXSensor::read_registers(uint8_t address, uint16_t reg, uint8_t *data, uint16_t len) {
  const uint8_t prefix[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg)};
  return this->bus_->write_readv(address, prefix, sizeof(prefix), data, len) == i2c::ERROR_OK;
}

uint32_t VL53L4CXSensor::millis() { return esphome::millis(); }
void VL53L4CXSensor::delay_ms(uint32_t ms) { delay(ms); }
void VL53L4CXSensor::delay_us(uint32_t us) { delayMicroseconds(us); }

void VL53L4CXSensor::setup() {
  // The first sensor to set up holds every sensor with an enable pin in reset, so each one
  // comes back at the default address only when its own setup() releases it.
  if (VL53L4CXSensor::first_sensor != nullptr) {
    for (auto *other = VL53L4CXSensor::first_sensor; other != nullptr; other = other->next_) {
      setup_output_pin(other->enable_pin_, false);
    }
    VL53L4CXSensor::first_sensor = nullptr;
  }
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
  }
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
  }

  if (!this->init_sensor_())
    return;

  // ST's driver ranges again only once the interrupt is cleared, so update() paces the measurements.
  if (!this->setup_check(this->driver_.VL53L4CX_StartMeasurement() == VL53L4CX_ERROR_NONE))
    return;
  this->measurement_start_ = esphome::millis();
}

bool VL53L4CXSensor::init_sensor_() {
  if (this->driver_.VL53L4CX_WaitDeviceBooted() != VL53L4CX_ERROR_NONE) {
    this->fail_at_default_address_(LOG_STR("Boot timeout"));
    return false;
  }
  uint16_t sensor_id;
  if (this->driver_.VL53L4CX_GetSensorId(&sensor_id) != VL53L4CX_ERROR_NONE) {
    this->fail_at_default_address_(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  if (sensor_id != SENSOR_ID) {
    ESP_LOGE(TAG, "Wrong sensor ID 0x%04X", sensor_id);
    this->fail_at_default_address_(LOG_STR("Wrong sensor ID"));
    return false;
  }
  // The driver takes the 8-bit form of the address
  if (this->address_ != DEFAULT_ADDRESS &&
      this->driver_.VL53L4CX_SetDeviceAddress(this->address_ << 1) != VL53L4CX_ERROR_NONE) {
    this->fail_at_default_address_(LOG_STR("Setting address failed"));
    return false;
  }
  return this->setup_check(this->driver_.VL53L4CX_DataInit() == VL53L4CX_ERROR_NONE &&
                           this->driver_.VL53L4CX_SetDistanceMode(this->distance_mode_) == VL53L4CX_ERROR_NONE &&
                           this->driver_.VL53L4CX_SetMeasurementTimingBudgetMicroSeconds(this->budget_ms_ * 1000UL) ==
                               VL53L4CX_ERROR_NONE);
}

void VL53L4CXSensor::fail_at_default_address_(const LogString *message) {
  if (this->enable_pin_ != nullptr)
    this->enable_pin_->digital_write(false);
  this->mark_failed(message);
}

void VL53L4CXSensor::update() {
  if (this->is_in_loop_state()) {
    ESP_LOGD(TAG, "Measurement still running");
    return;
  }
  if (this->driver_.VL53L4CX_ClearInterruptAndStartMeasurement() != VL53L4CX_ERROR_NONE) {
    ESP_LOGW(TAG, "Starting measurement failed");
    this->status_set_warning();
    this->publish_state(NAN);
    return;
  }
  this->measurement_start_ = App.get_loop_component_start_time();
  this->enable_loop();
}

bool VL53L4CXSensor::measurement_ready_() {
  // GPIO1 stays low from the end of a measurement until update() clears it
  if (this->interrupt_pin_ != nullptr)
    return !this->interrupt_pin_->digital_read();
  uint8_t ready = 0;
  return this->driver_.VL53L4CX_GetMeasurementDataReady(&ready) == VL53L4CX_ERROR_NONE && ready;
}

void VL53L4CXSensor::loop() {
  const uint32_t elapsed = App.get_loop_component_start_time() - this->measurement_start_;
  if (elapsed < this->budget_ms_)
    return;
  if (!this->measurement_ready_()) {
    if (elapsed > static_cast<uint32_t>(this->budget_ms_) * 2 + 100) {
      ESP_LOGW(TAG, "Measurement timed out");
      this->status_set_warning();
      this->publish_state(NAN);
      this->disable_loop();
    }
    return;
  }
  this->disable_loop();
  this->publish_result_();
}

int32_t select_distance_mm(const VL53L4CX_MultiRangingData_t &data, Target target, uint16_t min_distance_mm,
                           int16_t offset_mm) {
  int32_t best_mm = -1;
  FixPoint1616_t best_signal = 0;
  for (uint8_t i = 0; i < data.NumberOfObjectsFound && i < VL53L4CX_MAX_RANGE_RESULTS; i++) {
    const VL53L4CX_TargetRangeData_t &range = data.RangeData[i];
    if (range.RangeStatus != VL53L4CX_RANGESTATUS_RANGE_VALID)
      continue;
    const int32_t mm = range.RangeMilliMeter + offset_mm;
    if (mm < min_distance_mm)
      continue;
    const bool better = best_mm < 0 || (target == TARGET_NEAREST && mm < best_mm) ||
                        (target == TARGET_FARTHEST && mm > best_mm) ||
                        (target == TARGET_STRONGEST && range.SignalRateRtnMegaCps > best_signal);
    if (better) {
      best_mm = mm;
      best_signal = range.SignalRateRtnMegaCps;
    }
  }
  return best_mm;
}

void VL53L4CXSensor::publish_result_() {
  VL53L4CX_MultiRangingData_t data;
  if (!this->status_comm_result(this->driver_.VL53L4CX_GetMultiRangingData(&data) == VL53L4CX_ERROR_NONE)) {
    this->publish_state(NAN);
    return;
  }
  const int32_t best_mm = select_distance_mm(data, this->target_, this->min_distance_mm_, this->offset_mm_);
  ESP_LOGV(TAG, "%u targets, reporting %" PRId32 " mm", data.NumberOfObjectsFound, best_mm);
  this->publish_state(best_mm < 0 ? NAN : best_mm / 1000.0f);
}

void VL53L4CXSensor::dump_config() {
  LOG_SENSOR("", "VL53L4CX", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  ESP_LOGCONFIG(TAG,
                "  Distance Mode: %s\n"
                "  Timing Budget: %u ms\n"
                "  Target: %s\n"
                "  Minimum Distance: %u mm\n"
                "  Offset: %d mm",
                LOG_STR_ARG(DistanceModeStrings::get_log_str(this->distance_mode_)), this->budget_ms_,
                LOG_STR_ARG(TargetStrings::get_log_str(this->target_)), this->min_distance_mm_, this->offset_mm_);
}

}  // namespace esphome::vl53l4cx

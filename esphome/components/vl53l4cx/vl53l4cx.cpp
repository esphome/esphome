#include "vl53l4cx.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>

namespace esphome::vl53l4cx {

ESPHOME_LOG_TAG(TAG, "vl53l4cx");

static constexpr uint8_t DEFAULT_ADDRESS = 0x29;
// Model id 0xEB and module type 0xAA at IDENTIFICATION__MODEL_ID
static constexpr uint16_t SENSOR_ID = 0xEBAA;
// Failed measurements in a row after which the state becomes NaN, so a dead sensor does not keep its last value
static constexpr uint8_t MAX_FAILURES = 5;

PROGMEM_STRING_TABLE(TargetStrings, "nearest", "farthest", "strongest");
// Indexed by ST's VL53L4CX_DISTANCEMODE_* from medium; the VL53L4CX has no short mode
PROGMEM_STRING_TABLE(DistanceModeStrings, "medium", "long");

VL53L4CXSensor *VL53L4CXSensor::first_sensor = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

VL53L4CXSensor::VL53L4CXSensor() {
  this->next_ = VL53L4CXSensor::first_sensor;
  VL53L4CXSensor::first_sensor = this;
}

static constexpr size_t REGISTER_ADDRESS_SIZE = 2;
// ESP8266's Arduino Wire buffers 128 bytes per transaction and ST's driver writes up to 135 at once. The
// register address auto-increments, so longer transfers are split, each part at its own address.
#ifdef USE_ESP8266
// A write sends the register address from the same buffer; reads use the write limit too
static constexpr uint16_t MAX_TRANSFER = 128 - REGISTER_ADDRESS_SIZE;
#else
static constexpr uint16_t MAX_TRANSFER = UINT16_MAX;
#endif

/// Calls transfer(prefix, offset, part) for each part of a `len` byte transfer starting at register `reg`.
template<typename F> static bool transfer_in_parts(uint16_t reg, uint16_t len, F &&transfer) {
  uint16_t done = 0;
  do {
    const uint16_t part = std::min<uint16_t>(len - done, MAX_TRANSFER);
    uint8_t prefix[REGISTER_ADDRESS_SIZE];
    put_be16(prefix, reg + done);
    if (!transfer(prefix, done, part))
      return false;
    done += part;
  } while (done < len);
  return true;
}

bool VL53L4CXSensor::write_registers(uint8_t address, uint16_t reg, const uint8_t *data, uint16_t len) {
  return transfer_in_parts(reg, len, [&](const uint8_t *prefix, uint16_t offset, uint16_t part) {
    return this->bus_->write_prefixed(address, prefix, REGISTER_ADDRESS_SIZE, data + offset, part) == i2c::ERROR_OK;
  });
}

bool VL53L4CXSensor::read_registers(uint8_t address, uint16_t reg, uint8_t *data, uint16_t len) {
  return transfer_in_parts(reg, len, [&](const uint8_t *prefix, uint16_t offset, uint16_t part) {
    return this->bus_->write_readv(address, prefix, REGISTER_ADDRESS_SIZE, data + offset, part) == i2c::ERROR_OK;
  });
}

uint32_t VL53L4CXSensor::millis() { return esphome::millis(); }
void VL53L4CXSensor::delay_ms(uint32_t ms) { delay(ms); }
void VL53L4CXSensor::delay_us(uint32_t us) { delayMicroseconds(us); }

void VL53L4CXSensor::setup() {
  // The first sensor to set up holds every sensor with an enable pin in reset, so each one
  // comes back at the default address only when its own setup() releases it.
  for (auto *other = VL53L4CXSensor::first_sensor; other != nullptr; other = other->next_) {
    setup_output_pin(other->enable_pin_, false);
  }
  VL53L4CXSensor::first_sensor = nullptr;
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
  }
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
  }

  if (!this->init_sensor_())
    return;

  // ST's driver ranges again only once the interrupt is cleared: by loop() straight after each result when
  // update_interval is never, else by update().
  if (!this->setup_check(this->driver_.VL53L4CX_StartMeasurement() == VL53L4CX_ERROR_NONE))
    return;
  this->measurement_start_ = esphome::millis();
  this->measuring_ = true;
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

bool VL53L4CXSensor::start_measurement_() {
  this->measurement_start_ = App.get_loop_component_start_time();
  const VL53L4CX_Error error = this->driver_.VL53L4CX_ClearInterruptAndStartMeasurement();
  this->measuring_ = error == VL53L4CX_ERROR_NONE;
  if (!this->measuring_)
    this->measurement_failed_(LOG_STR("Starting a measurement failed"), error);
  return this->measuring_;
}

void VL53L4CXSensor::measurement_failed_(const LogString *message, VL53L4CX_Error error) {
  ESP_LOGV(TAG, "%s: error %d", LOG_STR_ARG(message), error);
  this->status_set_warning(message);
  if (this->failures_ < MAX_FAILURES) {
    this->failures_++;
    if (this->failures_ == MAX_FAILURES)
      this->publish_state(NAN);
  }
}

void VL53L4CXSensor::update() {
  if (this->measuring_) {
    ESP_LOGV(TAG, "Measurement still running");
    return;
  }
  if (this->start_measurement_())
    this->enable_loop();
}

void VL53L4CXSensor::loop() {
  const uint32_t elapsed = App.get_loop_component_start_time() - this->measurement_start_;
  if (elapsed < this->budget_ms_)
    return;
  if (!this->measuring_) {
    // Only continuous mode keeps the loop running without a measurement: retry the start
    if (elapsed > this->timeout_ms_())
      this->start_measurement_();
    return;
  }
  // GPIO1 stays low from the end of a measurement until the next start clears it
  uint8_t ready = 0;
  VL53L4CX_Error error = VL53L4CX_ERROR_NONE;
  if (this->interrupt_pin_ != nullptr) {
    ready = !this->interrupt_pin_->digital_read();
  } else {
    error = this->driver_.VL53L4CX_GetMeasurementDataReady(&ready);
  }
  if (ready) {
    this->measuring_ = false;
    this->publish_result_();
  } else if (error != VL53L4CX_ERROR_NONE) {
    this->measuring_ = false;
    this->measurement_failed_(LOG_STR("Polling data ready failed"), error);
  } else if (elapsed > this->timeout_ms_()) {
    this->measuring_ = false;
    this->measurement_failed_(LOG_STR("Measurement timed out"), VL53L4CX_ERROR_TIME_OUT);
  } else {
    return;
  }
  if (this->get_update_interval() != SCHEDULER_DONT_RUN) {
    this->disable_loop();
    return;
  }
  this->start_measurement_();
}

static bool is_valid_range(uint8_t status) {
  // ST's valid statuses: plain, clipped to the minimum range, the first reading (no wrap check yet), merged pulses
  return status == VL53L4CX_RANGESTATUS_RANGE_VALID || status == VL53L4CX_RANGESTATUS_RANGE_VALID_MIN_RANGE_CLIPPED ||
         status == VL53L4CX_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK_FAIL ||
         status == VL53L4CX_RANGESTATUS_RANGE_VALID_MERGED_PULSE;
}

int32_t select_distance_mm(const VL53L4CX_MultiRangingData_t &data, Target target, uint16_t min_distance_mm,
                           int16_t offset_mm) {
  int32_t best_mm = -1;
  FixPoint1616_t best_signal = 0;
  for (uint8_t i = 0; i < data.NumberOfObjectsFound && i < VL53L4CX_MAX_RANGE_RESULTS; i++) {
    const VL53L4CX_TargetRangeData_t &range = data.RangeData[i];
    if (!is_valid_range(range.RangeStatus))
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
  const VL53L4CX_Error error = this->driver_.VL53L4CX_GetMultiRangingData(&data);
  if (error != VL53L4CX_ERROR_NONE) {
    this->measurement_failed_(LOG_STR("Reading the measurement failed"), error);
    return;
  }
  this->failures_ = 0;
  this->status_clear_warning();
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
                LOG_STR_ARG(DistanceModeStrings::get_log_str(this->distance_mode_ - VL53L4CX_DISTANCEMODE_MEDIUM)),
                this->budget_ms_, LOG_STR_ARG(TargetStrings::get_log_str(this->target_)), this->min_distance_mm_,
                this->offset_mm_);
}

}  // namespace esphome::vl53l4cx

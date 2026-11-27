#include "vl53l4cx.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>
#include <new>

namespace esphome::vl53l4cx {

ESPHOME_LOG_TAG(TAG, "vl53l4cx");

static constexpr uint8_t DEFAULT_ADDRESS = 0x29;
static constexpr uint16_t SENSOR_ID = 0xEBAA;
// ST's first frames after a start are unreliable
static constexpr uint8_t SETTLE_FRAMES = 5;
static constexpr uint32_t RESET_INTERVAL_ID = 0;
static constexpr uint32_t RESET_INTERVAL_MS = 5000;
// Write 0, wait, write 1: reboots the sensor like a power-up
static constexpr uint16_t SOFT_RESET_REGISTER = 0x0000;
static constexpr uint32_t SOFT_RESET_US = 100;

PROGMEM_STRING_TABLE(TargetStrings, "nearest", "farthest", "strongest");
PROGMEM_STRING_TABLE(DistanceModeStrings, "medium", "long");

#ifdef USE_VL53L4CX_ENABLE_PIN
VL53L4CXSensor *VL53L4CXSensor::first_sensor = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

VL53L4CXSensor::VL53L4CXSensor() {
  this->next_ = VL53L4CXSensor::first_sensor;
  VL53L4CXSensor::first_sensor = this;
}
#endif

static constexpr size_t REGISTER_ADDRESS_SIZE = 2;
// Arduino Wire buffers 128 bytes on ESP8266 and 64 on LibreTiny, including the register address; the driver
// writes up to 135, so longer transfers are split (the register address auto-increments)
#if defined(USE_ESP8266)
static constexpr uint16_t MAX_TRANSFER = 128 - REGISTER_ADDRESS_SIZE;
#elif defined(USE_LIBRETINY)
static constexpr uint16_t MAX_TRANSFER = 64 - REGISTER_ADDRESS_SIZE;
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

void VL53L4CXSensor::hold_in_reset_() {
#ifdef USE_VL53L4CX_ENABLE_PIN
  if (this->enable_pin_ != nullptr)
    this->enable_pin_->digital_write(false);
#endif
}

void VL53L4CXSensor::reset_failed_(const LogString *reason) {
  ESP_LOGV(TAG, "Reset failed: %s", LOG_STR_ARG(reason));
  if (this->failures_ == MAX_FAILURES) {
    this->failures_++;
    this->status_clear_warning();
    this->status_set_warning(reason);
  }
  this->hold_in_reset_();
}

void VL53L4CXSensor::setup() {
#ifdef USE_VL53L4CX_ENABLE_PIN
  // Hold every sensor in reset so each comes up at 0x29 only in its own setup()
  for (auto *other = VL53L4CXSensor::first_sensor; other != nullptr; other = other->next_) {
    setup_output_pin(other->enable_pin_, false);
  }
  VL53L4CXSensor::first_sensor = nullptr;
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
  }
#endif
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&VL53L4CXSensor::ready_isr, this, gpio::INTERRUPT_FALLING_EDGE);
  }
#endif

  if (const LogString *error = this->init_and_start_(true)) {
    this->hold_in_reset_();
    this->mark_failed(error);
  }
}

const LogString *VL53L4CXSensor::init_and_start_(bool at_boot) {
  if (this->driver_.VL53L4CX_WaitDeviceBooted() != VL53L4CX_ERROR_NONE)
    return LOG_STR("Boot timeout");
  uint16_t sensor_id;
  if (this->driver_.VL53L4CX_GetSensorId(&sensor_id) != VL53L4CX_ERROR_NONE)
    return LOG_STR(ESP_LOG_MSG_COMM_FAIL);
  if (sensor_id != SENSOR_ID) {
    if (at_boot) {
      ESP_LOGE(TAG, "Wrong sensor ID 0x%04X", sensor_id);
    } else {
      ESP_LOGV(TAG, "Wrong sensor ID 0x%04X", sensor_id);
    }
    return LOG_STR("Wrong sensor ID");
  }
  // The driver takes the 8-bit form of the address
  if (this->address_ != DEFAULT_ADDRESS &&
      this->driver_.VL53L4CX_SetDeviceAddress(this->address_ << 1) != VL53L4CX_ERROR_NONE)
    return LOG_STR("Setting address failed");
  if (this->driver_.VL53L4CX_DataInit() != VL53L4CX_ERROR_NONE)
    return LOG_STR("Initialization failed");
  if (this->driver_.VL53L4CX_SetDistanceMode(this->distance_mode_) != VL53L4CX_ERROR_NONE ||
      this->driver_.VL53L4CX_SetMeasurementTimingBudgetMicroSeconds(this->budget_ms_ * 1000UL) != VL53L4CX_ERROR_NONE)
    return LOG_STR("Configuration failed");
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  this->ready_edge_ = false;
#endif
  if (this->driver_.VL53L4CX_StartMeasurement() != VL53L4CX_ERROR_NONE)
    return LOG_STR("Starting measurements failed");
  this->measurement_start_ = esphome::millis();
  this->measuring_ = true;
  this->settle_frames_ = SETTLE_FRAMES;
  return nullptr;
}

void VL53L4CXSensor::reset_sensor_() {
  // Both resets leave the sensor unconfigured at 0x29, so start over with a fresh driver
#ifdef USE_VL53L4CX_ENABLE_PIN
  if (this->enable_pin_ != nullptr) {
    pulse_reset(this->enable_pin_, {.low_ms = 2});
  } else
#endif
  {
    // Always release; a sensor that was not reset must not run DataInit again
    const bool held = this->write_byte_reg16(SOFT_RESET_REGISTER, 0);
    delayMicroseconds(SOFT_RESET_US);
    if (!this->write_byte_reg16(SOFT_RESET_REGISTER, 1) || !held) {
      this->reset_failed_(LOG_STR("Software reset not written"));
      return;
    }
  }
  this->driver_.~VL53L4CX();
  new (&this->driver_) VL53L4CX(this);
  if (const LogString *error = this->init_and_start_(false)) {
    this->reset_failed_(error);
    return;
  }
  this->cancel_interval(RESET_INTERVAL_ID);
  this->failures_ = 0;
  this->enable_loop();
}

bool VL53L4CXSensor::start_measurement_() {
  this->measurement_start_ = App.get_loop_component_start_time();
  // The driver ranges again only once the interrupt is cleared
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  this->ready_edge_ = false;
#endif
  const VL53L4CX_Error error = this->driver_.VL53L4CX_ClearInterruptAndStartMeasurement();
  this->measuring_ = error == VL53L4CX_ERROR_NONE;
  if (!this->measuring_)
    this->measurement_failed_(LOG_STR("Starting a measurement failed"), error);
  return this->measuring_;
}

void VL53L4CXSensor::measurement_failed_(const LogString *message, VL53L4CX_Error error) {
  ESP_LOGV(TAG, "%s: error %d", LOG_STR_ARG(message), error);
  this->status_set_warning(message);
  if (++this->failures_ == MAX_FAILURES) {
    this->publish_state(NAN);
    this->set_interval(RESET_INTERVAL_ID, RESET_INTERVAL_MS, [this]() { this->reset_sensor_(); });
  }
}

void VL53L4CXSensor::update() {
  if (this->resetting_())
    return;
  if (this->measuring_) {
    ESP_LOGV(TAG, "Measurement still running");
    return;
  }
  if (this->start_measurement_())
    this->enable_loop();
}

void VL53L4CXSensor::loop() {
  const uint32_t elapsed = App.get_loop_component_start_time() - this->measurement_start_;
  if (!this->measuring_) {
    if (this->get_update_interval() != SCHEDULER_DONT_RUN || this->resetting_()) {
      this->disable_loop();
    } else if (elapsed > this->timeout_ms_()) {
      this->start_measurement_();
    }
    return;
  }
  const bool settling = this->settle_frames_ > 0;
  uint8_t ready = 0;
  VL53L4CX_Error error = VL53L4CX_ERROR_NONE;
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  // Edge and low level: a stuck-low pin makes no edge, and a stale edge leaves the pin high after the start
  if (this->interrupt_pin_ != nullptr) {
    ready = this->ready_edge_ && !this->interrupt_pin_->digital_read();
  } else
#endif
  {
    if (elapsed < this->budget_ms_)
      return;
    error = this->driver_.VL53L4CX_GetMeasurementDataReady(&ready);
  }
  // On a failed poll the driver can report ready from an unread byte
  if (error == VL53L4CX_ERROR_NONE && !ready && elapsed <= this->timeout_ms_())
    return;
  this->measuring_ = false;
  if (error != VL53L4CX_ERROR_NONE) {
    this->measurement_failed_(LOG_STR("Polling data ready failed"), error);
  } else if (ready) {
    this->publish_result_();
  } else {
    this->measurement_failed_(LOG_STR("Measurement timed out"), VL53L4CX_ERROR_TIME_OUT);
  }
  // An on_value automation may have called update() already
  if (this->measuring_)
    return;
  if ((this->get_update_interval() != SCHEDULER_DONT_RUN && !settling) || this->resetting_()) {
    this->disable_loop();
    return;
  }
  this->start_measurement_();
}

static bool is_valid_range(uint8_t status) {
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
  if (this->settle_frames_ > 0) {
    this->settle_frames_--;
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
#ifdef USE_VL53L4CX_ENABLE_PIN
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
#endif
#ifdef USE_VL53L4CX_INTERRUPT_PIN
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
#endif
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

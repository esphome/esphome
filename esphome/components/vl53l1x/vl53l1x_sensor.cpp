#include "vl53l1x_sensor.h"
#include "driver.h"

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/log.h"

#include <map>
#include <cassert>

namespace esphome {
namespace vl53l1x {

using driver::VL53L1X_ERROR;

static const char *const TAG = "vl53l1x";

// When using multiple VL53L1X sensors on the board, we require them to all
// have different addresses, and if multiple boards are on the same bus,
// they all need the enable_pin set.
// During setup we will disable all sensors, and bring them online
// one after another as we are bringing them online and register the updated
// I2C address with the sensor firmware.
constexpr ::uint8_t DEFAULT_I2C_ADDRESS = 0x29;

bool VL53L1xSensor::pin_setup_complete = false;
::std::list<VL53L1xSensor *> VL53L1xSensor::all_sensors;

VL53L1xSensor::VL53L1xSensor() { all_sensors.push_back(this); }

void VL53L1xSensor::setup() {
  VL53L1X_ERROR err = 0;
  if (!esphome::vl53l1x::VL53L1xSensor::pin_setup_complete) {
    ESP_LOGCONFIG(TAG, "Bootstrapping VL53L1x enable-pins...");
    // Disable all sensors that have enable_pins set.
    for (auto *sensor : esphome::vl53l1x::VL53L1xSensor::all_sensors) {
      sensor->setup_enable_pin_();
      sensor->disable_();
    }
    esphome::vl53l1x::VL53L1xSensor::pin_setup_complete = true;
  }
  ESP_LOGCONFIG(TAG, "Setting up VL53L1X...");
  ::esphome::delay(2);

  // Powercycle this sensor to reset firmware address to DEFAULT_I2C_ADDRESS
  this->enable_();
  ::esphome::delay(2);

  uint16_t final_i2c_address = this->get_i2c_address();

  if (final_i2c_address != DEFAULT_I2C_ADDRESS) {
    this->set_i2c_address(DEFAULT_I2C_ADDRESS);
  }

  this->initialized_ = this->init_sensor_();
  if (!this->initialized_) {
    this->mark_failed();
    return;
  }

  if (this->address_ != final_i2c_address) {
    // The first address argument is used to hook the currently configured the I2CDevice in the platform bridge.
    if ((err = driver::set_i2c_address(this, final_i2c_address << 1)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGE(TAG, "set_i2c_address failed: %d", err);
      this->mark_failed();
      return;
    }

    this->set_i2c_address(final_i2c_address);
  }

  // Apply configuration
  this->apply_distance_mode_();
  this->apply_timing_budget_();
  this->apply_update_interval_();
  this->apply_distance_threshold_();
  this->apply_roi_();

  if (interrupt_pin_ == nullptr) {
    // If interrupt_pin is null, then we are in polling mode and should schedule an
    // update at the same rate as as the time between measurements.
    // As timing can become skewed over time, we add a retry scheduled every quarter
    // measurement budget until next update.

    const uint32_t retry_interval = (this->measurement_timing_budget_ms_ + 3) / 4;
    const uint8_t retry_count = this->update_interval_ms_ / retry_interval;

    this->set_interval("update", this->update_interval_ms_, [this, retry_count, retry_interval]() {
      // Cancel any pending retry.
      this->cancel_retry("retry_update");

      auto retry_result = this->update();
      // If result is not ready yet, retry in a quarter timing_budget.
      if (retry_result == RetryResult::RETRY) {
        ESP_LOGD(TAG, "Measurement not ready in time, retrying in %d ms", retry_interval);

        this->set_retry(
            "retry_update", retry_interval, retry_count,
            [this](uint8_t count) {
              auto res = this->update();
              ESP_LOGD(TAG, "Retry #%d, result %s", count, res == RetryResult::DONE ? "Done" : "Retry");
              return res;
            },
            1.0);
      }
    });
  } else {
    // If interrupt-pin is set, then configure one iteration of Component::loop() to run each interrupt.
    interrupt_pin_->setup();
    interrupt_pin_->attach_interrupt(VL53L1xSensor::schedule_update_from_isr, this, gpio::INTERRUPT_RISING_EDGE);
  }

  // Start measurements
  if ((err = driver::start_ranging(this)) != driver::VL53L1X_ERROR_NONE) {
    ESP_LOGE(TAG, "start_ranging failed: %d", err);
    this->mark_failed();
  }
}

// Loop is called once per interrupt.
void VL53L1xSensor::loop() {
  this->cancel_timeout("clear_measurement");

  this->update();
  this->disable_loop();

  // Clear measurement after two intervals with no new event.
  this->set_timeout("clear_measurement", 2 * this->update_interval_ms_, [this]() { this->publish_state(NAN); });

  if (this->interrupt_pin_ != nullptr) {
    VL53L1X_ERROR err = 0;
    if ((err = driver::clear_interrupt(this)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "clear_interrupt failed %d", err);
    }
  }
}

void VL53L1xSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "VL53L1X:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Update Mode: %s", this->interrupt_pin_ != nullptr ? "interrupt-driven" : "polling");
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", (unsigned) this->update_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Timing Budget: %u ms", (unsigned) this->measurement_timing_budget_ms_);
  ESP_LOGCONFIG(TAG, "  Distance Mode: %u", (unsigned) this->distance_mode_);
  ESP_LOGCONFIG(TAG, "  Offset: %d mm", this->offset_);
  ESP_LOGCONFIG(TAG, "  XTalk Correction: %u cps", this->xtalk_correction_);
  if (this->sigma_threshold_ != 0xffff) {
    ESP_LOGCONFIG(TAG, "  Sigma Threshold: %u mm", (unsigned) this->sigma_threshold_);
  }
  if (this->signal_threshold_ != 0xffff) {
    ESP_LOGCONFIG(TAG, "  Signal Threshold: %u kcps", (unsigned) this->signal_threshold_);
  }

  if (this->distance_threshold_.interrupt_when != NOT_SET) {
    ESP_LOGCONFIG(TAG, "  Distance Threshold:");
    ESP_LOGCONFIG(TAG, "     min: %u mm", this->distance_threshold_.min);
    ESP_LOGCONFIG(TAG, "     max: %u mm", this->distance_threshold_.max);
    ESP_LOGCONFIG(TAG, "     interrupt_when: %d", this->distance_threshold_.interrupt_when);
  }

  if (this->roi_.isSet) {
    ESP_LOGCONFIG(TAG, "  Region of Interest:");
    ESP_LOGCONFIG(TAG, "     BottomLeft: (%u,%u)", this->roi_.x, this->roi_.y);
    ESP_LOGCONFIG(TAG, "     W/H: %u/%u", this->roi_.w, this->roi_.h);
  }

  if (this->enable_pin_ != nullptr) {
    LOG_PIN("  Enable Pin: ", this->enable_pin_);
  }
  if (this->interrupt_pin_ != nullptr) {
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication failed!");
  }
}

RetryResult VL53L1xSensor::update() {
  if (!this->initialized_) {
    this->publish_state(NAN);
    return RetryResult::DONE;
  }

  uint16_t distance_mm = 0;
  ReadResult read_result = this->read_distance_mm_(distance_mm);

  if (read_result == ReadResult::FAILURE) {
    this->publish_state(NAN);
    this->status_momentary_warning("read", 5000);
    return RetryResult::DONE;
  } else if (read_result == ReadResult::RETRY) {
    return RetryResult::RETRY;
  }
  const float distance_m = distance_mm / 1000.0f;
  ESP_LOGVV(TAG, "Distance: %.3f m", distance_m);
  this->publish_state(distance_m);
  return RetryResult::DONE;
}

void VL53L1xSensor::setup_enable_pin_() {
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
  }
}

bool VL53L1xSensor::enable_() {
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
    return true;
  }
  return false;
}

void VL53L1xSensor::disable_() {
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(false);
  }
}

bool VL53L1xSensor::init_sensor_() {
  uint8_t boot = 0;
  VL53L1X_ERROR err = 0;
  const char *failing_call = "";

  const uint32_t start_us = micros();
  while ((micros() - start_us) < 200000) {
    if ((err = driver::boot_state(this, &boot)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "boot_state failed %d", err);
    }
    if (boot)
      break;
    ESP_LOGVV(TAG, "boot_state: %d", boot);
    delay(2);
  }

  if (!boot) {
    ESP_LOGE(TAG, "Boot not completed: %d", err);
    return false;
  }

  if ((err = driver::sensor_init(this)) != 0) {
    failing_call = "sensor_init";
    goto setup_error;
  }

  if ((err = driver::start_temperature_update(this)) != 0) {
    failing_call = "start_temperature_update";
    goto setup_error;
  }
  return true;

setup_error:
  ESP_LOGE(TAG, "%s failed: %d", failing_call, err);
  return false;
}

static const char *range_status_to_str(uint8_t range_status) {
  switch (range_status) {
    case 0:
      return "ok";
    case 1:
      return "sigma failure";
    case 2:
      return "signal failure";
    case 4:
      return "too far away";
    case 7:
      return "wraparound";
    case 13:
      return "invalid region of interest configuration";
    default:
      return "unknown";
  }
}

VL53L1xSensor::ReadResult VL53L1xSensor::read_distance_mm_(uint16_t &distance_mm) {
  const char *failing_call = "";
  uint8_t err = 0, ready = 0, range_status = 0;
  uint16_t tmp_distance = 0;

  if ((err = driver::check_for_data_ready(this, &ready) != driver::VL53L1X_ERROR_NONE)) {
    failing_call = "check_for_data_ready";
    goto read_distance_error;
  }
  if (!ready) {
    return ReadResult::RETRY;
  }

  if ((err = driver::get_range_status(this, &range_status)) != driver::VL53L1X_ERROR_NONE) {
    failing_call = "get_range_status";
    goto read_distance_error;
  }
  if (range_status != 0) {
    ESP_LOGW(TAG, "Range failure: %s", range_status_to_str(range_status));
    return ReadResult::FAILURE;
  }

  if ((err = driver::get_distance(this, &tmp_distance)) != driver::VL53L1X_ERROR_NONE) {
    failing_call = "get_distance";
    goto read_distance_error;
  }
  distance_mm = tmp_distance;
  return ReadResult::SUCCESS;

read_distance_error:
  ESP_LOGE(TAG, "%s failed: %d", failing_call, err);
  return ReadResult::FAILURE;
}

bool VL53L1xSensor::apply_distance_mode_() {
  uint8_t err = 0;
  uint16_t vendor_mode = (this->distance_mode_ == DistanceMode::SHORT) ? 1 : 2;  // ULD supports short/long
  if ((err = driver::set_distance_mode(this, vendor_mode)) != driver::VL53L1X_ERROR_NONE) {
    ESP_LOGW(TAG, "set_distance_mode failed: %d", err);
    return false;
  }
  return true;
}

bool VL53L1xSensor::apply_timing_budget_() {
  uint8_t err = 0;
  if ((err = driver::set_timing_budget_in_ms(this, this->measurement_timing_budget_ms_)) !=
      driver::VL53L1X_ERROR_NONE) {
    ESP_LOGW(TAG, "set_timing_budget_in_ms failed: %d", err);
    return false;
  }
  return true;
}

bool VL53L1xSensor::apply_update_interval_() {
  uint8_t err = 0;
  if ((err = driver::set_inter_measurement_in_ms(this, this->update_interval_ms_)) != driver::VL53L1X_ERROR_NONE) {
    ESP_LOGW(TAG, "set_inter_measurement_in_ms failed: %d", err);
    return false;
  }
  return true;
}

bool VL53L1xSensor::apply_distance_threshold_() {
  if (this->distance_threshold_.interrupt_when != NOT_SET) {
    uint8_t err = 0;
    if ((err = driver::set_distance_threshold(this, this->distance_threshold_.min, this->distance_threshold_.max,
                                              this->distance_threshold_.interrupt_when)) !=
        driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "SetDistanceThreshold failed: %d", err);
      return false;
    }
  }
  return true;
}

static const uint8_t SPAD_INDEX_TABLE[16][16] = {
    {120, 112, 104, 96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0},
    {121, 113, 105, 97, 89, 81, 73, 65, 57, 49, 41, 33, 25, 17, 9, 1},
    {122, 114, 106, 98, 90, 82, 74, 66, 58, 50, 42, 34, 26, 18, 10, 2},
    {123, 115, 107, 99, 91, 83, 75, 67, 59, 51, 43, 35, 27, 19, 11, 3},
    {124, 116, 108, 100, 92, 84, 76, 68, 60, 52, 44, 36, 28, 20, 12, 4},
    {125, 117, 109, 101, 93, 85, 77, 69, 61, 53, 45, 37, 29, 21, 13, 5},
    {126, 118, 110, 102, 94, 86, 78, 70, 62, 54, 46, 38, 30, 22, 14, 6},
    {127, 119, 111, 103, 95, 87, 79, 71, 63, 55, 47, 39, 31, 23, 15, 7},

    {135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239, 247, 255},
    {134, 142, 150, 158, 166, 174, 182, 190, 198, 206, 214, 222, 230, 238, 246, 254},
    {133, 141, 149, 157, 165, 173, 181, 189, 197, 205, 213, 221, 229, 237, 245, 253},
    {132, 140, 148, 156, 164, 172, 180, 188, 196, 204, 212, 220, 228, 236, 244, 252},
    {131, 139, 147, 155, 163, 171, 179, 187, 195, 203, 211, 219, 227, 235, 243, 251},
    {130, 138, 146, 154, 162, 170, 178, 186, 194, 202, 210, 218, 226, 234, 242, 250},
    {129, 137, 145, 153, 161, 169, 177, 185, 193, 201, 209, 217, 225, 233, 241, 249},
    {128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248},
    /*origo*/
};

bool VL53L1xSensor::apply_roi_() {
  if (this->roi_.isSet) {
    uint8_t err = 0;

    uint8_t center_x = this->roi_.x + this->roi_.w / 2;
    uint8_t center_y = this->roi_.y + this->roi_.h / 2;

    if ((err = driver::set_roi(this, this->roi_.w, this->roi_.h)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "set_roi failed: %d", err);
      return false;
    }

    if ((err = driver::set_roi_center(this, SPAD_INDEX_TABLE[center_y][center_x])) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "set_roi_center failed: %d", err);
      return false;
    }
  }
  return true;
}

bool VL53L1xSensor::apply_offset_() {
  if (this->offset_ != 0) {
    uint8_t err = 0;
    if ((err = driver::set_offset(this, this->offset_)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "set_offset failed: %d", err);
      return false;
    }
  }
  return true;
}

bool VL53L1xSensor::apply_xtalk_correction_() {
  if (this->xtalk_correction_ != 0) {
    uint8_t err = 0;
    if ((err = driver::set_xtalk(this, this->xtalk_correction_)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "set_xtalk failed: %d", err);
      return false;
    }
  }
  return true;
}

bool VL53L1xSensor::apply_sigma_threshold_() {
  if (this->sigma_threshold_ != 0xffff) {
    uint8_t err = 0;
    if ((err = driver::set_sigma_threshold(this, this->sigma_threshold_)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "set_sigma_threshold failed: %d", err);
      return false;
    }
  }
  return true;
}

bool VL53L1xSensor::apply_signal_threshold_() {
  if (this->signal_threshold_ != 0xffff) {
    uint8_t err = 0;
    if ((err = driver::set_signal_threshold(this, this->signal_threshold_)) != driver::VL53L1X_ERROR_NONE) {
      ESP_LOGW(TAG, "set_signal_threshold failed: %d", err);
      return false;
    }
  }
  return true;
}

}  // namespace vl53l1x

}  // namespace esphome

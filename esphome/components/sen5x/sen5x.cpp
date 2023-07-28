#include "sen5x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen5x {

static const char *const TAG = "sen5x";

static const uint16_t SEN5X_CMD_AUTO_CLEANING_INTERVAL = 0x8004;
static const uint16_t SEN5X_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SEN5X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static const uint16_t SEN5X_CMD_GET_PRODUCT_NAME = 0xD014;
static const uint16_t SEN5X_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN5X_CMD_NOX_ALGORITHM_TUNING = 0x60E1;
static const uint16_t SEN5X_CMD_READ_MEASUREMENT = 0x03C4;
static const uint16_t SEN5X_CMD_RHT_ACCELERATION_MODE = 0x60F7;
static const uint16_t SEN5X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY = 0x0037;
static const uint16_t SEN5X_CMD_STOP_MEASUREMENTS = 0x3f86;
static const uint16_t SEN5X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;

void SEN5XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen5x...");

  // the sensor needs 1000 ms to enter the idle state
  this->set_timeout(1000, [this]() {
    // Check if measurement is ready before reading the value
    if (!this->write_command(SEN5X_CMD_GET_DATA_READY_STATUS)) {
      ESP_LOGE(TAG, "Failed to write data ready status command");
      this->mark_failed();
      return;
    }

    uint16_t raw_read_status;
    if (!this->read_data(raw_read_status)) {
      ESP_LOGE(TAG, "Failed to read data ready status");
      this->mark_failed();
      return;
    }

    uint32_t stop_measurement_delay = 0;
    // In order to query the device periodic measurement must be ceased
    if (raw_read_status) {
      ESP_LOGD(TAG, "Sensor has data available, stopping periodic measurement");
      if (!this->write_command(SEN5X_CMD_STOP_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Failed to stop measurements");
        this->mark_failed();
        return;
      }
      // According to the SEN5x datasheet the sensor will only respond to other commands after waiting 200 ms after
      // issuing the stop_periodic_measurement command
      stop_measurement_delay = 200;
    }
    this->set_timeout(stop_measurement_delay, [this]() {
      uint16_t raw_serial_number[3];
      if (!this->get_register(SEN5X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 3, 20)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      this->serial_number_[0] = static_cast<bool>(uint16_t(raw_serial_number[0]) & 0xFF);
      this->serial_number_[1] = static_cast<uint16_t>(raw_serial_number[0] & 0xFF);
      this->serial_number_[2] = static_cast<uint16_t>(raw_serial_number[1] >> 8);
      ESP_LOGD(TAG, "Serial number %02d.%02d.%02d", serial_number_[0], serial_number_[1], serial_number_[2]);

      uint16_t raw_product_name[16];
      if (!this->get_register(SEN5X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
        ESP_LOGE(TAG, "Failed to read product name");
        this->error_code_ = PRODUCT_NAME_FAILED;
        this->mark_failed();
        return;
      }
      // 2 ASCII bytes are encoded in an int
      const uint16_t *current_int = raw_product_name;
      char current_char;
      uint8_t max = 16;
      do {
        // first char
        current_char = *current_int >> 8;
        if (current_char) {
          product_name_.push_back(current_char);
          // second char
          current_char = *current_int & 0xFF;
          if (current_char)
            product_name_.push_back(current_char);
        }
        current_int++;
      } while (current_char && --max);

      Sen5xType sen5x_type = UNKNOWN;
      if (product_name_ == "SEN50") {
        sen5x_type = SEN50;
      } else {
        if (product_name_ == "SEN54") {
          sen5x_type = SEN54;
        } else {
          if (product_name_ == "SEN55") {
            sen5x_type = SEN55;
          }
        }
        ESP_LOGD(TAG, "Productname %s", product_name_.c_str());
      }
      if (this->humidity_sensor_ && sen5x_type == SEN50) {
        ESP_LOGE(TAG, "For Relative humidity a SEN54 OR SEN55 is required. You are using a <%s> sensor",
                 this->product_name_.c_str());
        this->humidity_sensor_ = nullptr;  // mark as not used
      }
      if (this->temperature_sensor_ && sen5x_type == SEN50) {
        ESP_LOGE(TAG, "For Temperature a SEN54 OR SEN55 is required. You are using a <%s> sensor",
                 this->product_name_.c_str());
        this->temperature_sensor_ = nullptr;  // mark as not used
      }
      if (this->voc_sensor_ && sen5x_type == SEN50) {
        ESP_LOGE(TAG, "For VOC a SEN54 OR SEN55 is required. You are using a <%s> sensor", this->product_name_.c_str());
        this->voc_sensor_ = nullptr;  // mark as not used
      }
      if (this->nox_sensor_ && sen5x_type != SEN55) {
        ESP_LOGE(TAG, "For NOx a SEN55 is required. You are using a <%s> sensor", this->product_name_.c_str());
        this->nox_sensor_ = nullptr;  // mark as not used
      }

      if (!this->get_register(SEN5X_CMD_GET_FIRMWARE_VERSION, this->firmware_version_, 20)) {
        ESP_LOGE(TAG, "Failed to read firmware version");
        this->error_code_ = FIRMWARE_FAILED;
        this->mark_failed();
        return;
      }
      this->firmware_version_ >>= 8;
      ESP_LOGD(TAG, "Firmware version %d", this->firmware_version_);

      if (this->voc_sensor_ && this->store_baseline_) {
        // Hash with compilation time
        // This ensures the baseline storage is cleared after OTA
        uint32_t hash = fnv1_hash(App.get_compilation_time());
        this->pref_ = global_preferences->make_preference<Sen5xBaselines>(hash, true);

        if (this->pref_.load(&this->voc_baselines_storage_)) {
          ESP_LOGI(TAG, "Loaded VOC baseline state0: 0x%04" PRIX32 ", state1: 0x%04" PRIX32,
                   this->voc_baselines_storage_.state0, voc_baselines_storage_.state1);
        }

        // Initialize storage timestamp
        this->seconds_since_last_store_ = 0;

        if (this->voc_baselines_storage_.state0 > 0 && this->voc_baselines_storage_.state1 > 0) {
          ESP_LOGI(TAG, "Setting VOC baseline from save state0: 0x%04" PRIX32 ", state1: 0x%04" PRIX32,
                   this->voc_baselines_storage_.state0, voc_baselines_storage_.state1);
          uint16_t states[4];

          states[0] = voc_baselines_storage_.state0 >> 16;
          states[1] = voc_baselines_storage_.state0 & 0xFFFF;
          states[2] = voc_baselines_storage_.state1 >> 16;
          states[3] = voc_baselines_storage_.state1 & 0xFFFF;

          if (!this->write_command(SEN5X_CMD_VOC_ALGORITHM_STATE, states, 4)) {
            ESP_LOGE(TAG, "Failed to set VOC baseline from saved state");
          }
        }
      }
      bool result;
      if (this->auto_cleaning_interval_.has_value()) {
        // override default value
        result = write_command(SEN5X_CMD_AUTO_CLEANING_INTERVAL, this->auto_cleaning_interval_.value());
      } else {
        result = write_command(SEN5X_CMD_AUTO_CLEANING_INTERVAL);
      }
      if (result) {
        delay(20);
        uint16_t secs[2];
        if (this->read_data(secs, 2)) {
          auto_cleaning_interval_ = secs[0] << 16 | secs[1];
        }
      }
      if (acceleration_mode_.has_value()) {
        result = this->write_command(SEN5X_CMD_RHT_ACCELERATION_MODE, acceleration_mode_.value());
      } else {
        result = this->write_command(SEN5X_CMD_RHT_ACCELERATION_MODE);
      }
      if (!result) {
        ESP_LOGE(TAG, "Failed to set rh/t acceleration mode");
        this->error_code_ = COMMUNICATION_FAILED;
        this->mark_failed();
        return;
      }
      delay(20);
      if (!acceleration_mode_.has_value()) {
        uint16_t mode;
        if (this->read_data(mode)) {
          this->acceleration_mode_ = RhtAccelerationMode(mode);
        } else {
          ESP_LOGE(TAG, "Failed to read RHT Acceleration mode");
        }
      }
      if (this->voc_tuning_params_.has_value())
        this->write_tuning_parameters_(SEN5X_CMD_VOC_ALGORITHM_TUNING, this->voc_tuning_params_.value());
      if (this->nox_tuning_params_.has_value())
        this->write_tuning_parameters_(SEN5X_CMD_NOX_ALGORITHM_TUNING, this->nox_tuning_params_.value());

      if (this->temperature_compensation_.has_value())
        this->write_temperature_compensation_(this->temperature_compensation_.value());

      // Finally start sensor measurements
      auto cmd = SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY;
      if (this->pm_1_0_sensor_ || this->pm_2_5_sensor_ || this->pm_4_0_sensor_ || this->pm_10_0_sensor_) {
        // if any of the gas sensors are active we need a full measurement
        cmd = SEN5X_CMD_START_MEASUREMENTS;
      }

      if (!this->write_command(cmd)) {
        ESP_LOGE(TAG, "Error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
      initialized_ = true;
      ESP_LOGD(TAG, "Sensor initialized");
    });
  });
}

void SEN5XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "sen5x:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement Initialization failed!");
        break;
      case SERIAL_NUMBER_IDENTIFICATION_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor serial id");
        break;
      case PRODUCT_NAME_FAILED:
        ESP_LOGW(TAG, "Unable to read product name");
        break;
      case FIRMWARE_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  ESP_LOGCONFIG(TAG, "  Productname: %s", this->product_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Firmware version: %d", this->firmware_version_);
  ESP_LOGCONFIG(TAG, "  Serial number %02d.%02d.%02d", serial_number_[0], serial_number_[1], serial_number_[2]);
  if (this->auto_cleaning_interval_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Auto cleaning interval %" PRId32 " seconds", auto_cleaning_interval_.value());
  }
  if (this->acceleration_mode_.has_value()) {
    switch (this->acceleration_mode_.value()) {
      case LOW_ACCELERATION:
        ESP_LOGCONFIG(TAG, "  Low RH/T acceleration mode");
        break;
      case MEDIUM_ACCELERATION:
        ESP_LOGCONFIG(TAG, "  Medium RH/T accelertion mode");
        break;
      case HIGH_ACCELERATION:
        ESP_LOGCONFIG(TAG, "  High RH/T accelertion mode");
        break;
    }
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "PM  1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM  2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM  4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);  // SEN54 and SEN55 only
  LOG_SENSOR("  ", "NOx", this->nox_sensor_);  // SEN55 only
}

void SEN5XComponent::update() {
  if (!initialized_) {
    return;
  }

  // Store baselines after defined interval or if the difference between current and stored baseline becomes too
  // much
  if (this->store_baseline_ && this->seconds_since_last_store_ > SHORTEST_BASELINE_STORE_INTERVAL) {
    if (this->write_command(SEN5X_CMD_VOC_ALGORITHM_STATE)) {
      // run it a bit later to avoid adding a delay here
      this->set_timeout(550, [this]() {
        uint16_t states[4];
        if (this->read_data(states, 4)) {
          uint32_t state0 = states[0] << 16 | states[1];
          uint32_t state1 = states[2] << 16 | states[3];
          if ((uint32_t) std::abs(static_cast<int32_t>(this->voc_baselines_storage_.state0 - state0)) >
                  MAXIMUM_STORAGE_DIFF ||
              (uint32_t) std::abs(static_cast<int32_t>(this->voc_baselines_storage_.state1 - state1)) >
                  MAXIMUM_STORAGE_DIFF) {
            this->seconds_since_last_store_ = 0;
            this->voc_baselines_storage_.state0 = state0;
            this->voc_baselines_storage_.state1 = state1;

            if (this->pref_.save(&this->voc_baselines_storage_)) {
              ESP_LOGI(TAG, "Stored VOC baseline state0: 0x%04" PRIX32 " ,state1: 0x%04" PRIX32,
                       this->voc_baselines_storage_.state0, voc_baselines_storage_.state1);
            } else {
              ESP_LOGW(TAG, "Could not store VOC baselines");
            }
          }
        }
      });
    }
  }

  if (!this->write_command(SEN5X_CMD_READ_MEASUREMENT)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "write error read measurement (%d)", this->last_error_);
    return;
  }
  this->set_timeout(20, [this]() {
    uint16_t measurements[8];

    if (!this->read_data(measurements, 8)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "read data error (%d)", this->last_error_);
      return;
    }
    float pm_1_0 = measurements[0] / 10.0;
    if (measurements[0] == 0xFFFF)
      pm_1_0 = NAN;
    float pm_2_5 = measurements[1] / 10.0;
    if (measurements[1] == 0xFFFF)
      pm_2_5 = NAN;
    float pm_4_0 = measurements[2] / 10.0;
    if (measurements[2] == 0xFFFF)
      pm_4_0 = NAN;
    float pm_10_0 = measurements[3] / 10.0;
    if (measurements[3] == 0xFFFF)
      pm_10_0 = NAN;
    float humidity = measurements[4] / 100.0;
    if (measurements[4] == 0xFFFF)
      humidity = NAN;
    float temperature = measurements[5] / 200.0;
    if (measurements[5] == 0xFFFF)
      temperature = NAN;
    float voc = measurements[6] / 10.0;
    if (measurements[6] == 0xFFFF)
      voc = NAN;
    float nox = measurements[7] / 10.0;
    if (measurements[7] == 0xFFFF)
      nox = NAN;

    if (this->pm_1_0_sensor_ != nullptr)
      this->pm_1_0_sensor_->publish_state(pm_1_0);
    if (this->pm_2_5_sensor_ != nullptr)
      this->pm_2_5_sensor_->publish_state(pm_2_5);
    if (this->pm_4_0_sensor_ != nullptr)
      this->pm_4_0_sensor_->publish_state(pm_4_0);
    if (this->pm_10_0_sensor_ != nullptr)
      this->pm_10_0_sensor_->publish_state(pm_10_0);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    if (this->voc_sensor_ != nullptr)
      this->voc_sensor_->publish_state(voc);
    if (this->nox_sensor_ != nullptr)
      this->nox_sensor_->publish_state(nox);
    this->status_clear_warning();
  });
}

bool SEN5XComponent::write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning) {
  uint16_t params[6];
  params[0] = tuning.index_offset;
  params[1] = tuning.learning_time_offset_hours;
  params[2] = tuning.learning_time_gain_hours;
  params[3] = tuning.gating_max_duration_minutes;
  params[4] = tuning.std_initial;
  params[5] = tuning.gain_factor;
  auto result = write_command(i2c_command, params, 6);
  if (!result) {
    ESP_LOGE(TAG, "set tuning parameters failed. i2c command=%0xX, err=%d", i2c_command, this->last_error_);
  }
  return result;
}

bool SEN5XComponent::write_temperature_compensation_(const TemperatureCompensation &compensation) {
  uint16_t params[3];
  params[0] = compensation.offset;
  params[1] = compensation.normalized_offset_slope;
  params[2] = compensation.time_constant;
  if (!write_command(SEN5X_CMD_TEMPERATURE_COMPENSATION, params, 3)) {
    ESP_LOGE(TAG, "set temperature_compensation failed. Err=%d", this->last_error_);
    return false;
  }
  return true;
}

bool SEN5XComponent::start_fan_cleaning() {
  if (!write_command(SEN5X_CMD_START_CLEANING_FAN)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start fan (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Fan auto clean started");
  }
  return true;
}

}  // namespace sen5x
}  // namespace esphome

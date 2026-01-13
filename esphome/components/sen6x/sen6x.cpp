#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::sen6x {

static const char *const TAG = "sen6x";

static const uint16_t SEN6X_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SEN6X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static const uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static const uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN6X_CMD_NOX_ALGORITHM_TUNING = 0x60E1;

static const uint16_t SEN62_CMD_READ_MEASUREMENT = 0x04A3;   // SEN62, returns 6 words
static const uint16_t SEN63C_CMD_READ_MEASUREMENT = 0x0471;  // SEN63C, returns 7 words
static const uint16_t SEN65_CMD_READ_MEASUREMENT = 0x0446;   // SEN65, returns 8 words
static const uint16_t SEN66_CMD_READ_MEASUREMENT = 0x0300;   // SEN66, returns 9 words
static const uint16_t SEN68_CMD_READ_MEASUREMENT = 0x0467;   // SEN68, returns 9 words
static const uint16_t SEN69C_CMD_READ_MEASUREMENT = 0x04B5;  // SEN69C, returns 10 words

static const uint16_t SEN6X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN6X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN6X_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SEN6X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t SEN6X_CMD_PRESSURE_COMPENSATION = 0x6720;
static const uint16_t SEN6X_CMD_ALTITUDE_COMPENSATION = 0x6736;
static const uint16_t SEN6X_CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t SEN6X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;

static const uint16_t SEN6X_CMD_PERFORM_FORCED_CO2_CALIBRATION = 0x0607;

static const int8_t SEN6X_INDEX_SCALE_FACTOR = 10;                            // used for VOC and NOx index values
static const int8_t SEN6X_MIN_INDEX_VALUE = 1 * SEN6X_INDEX_SCALE_FACTOR;     // must be adjusted by the scale factor
static const int16_t SEN6X_MAX_INDEX_VALUE = 500 * SEN6X_INDEX_SCALE_FACTOR;  // must be adjusted by the scale factor

void SEN6XComponent::setup() {
  // the sensor needs 1000 ms to enter the idle state
  this->set_timeout(1000, [this]() {
    // Check if measurement is ready before reading the value
    if (!this->write_command(SEN6X_CMD_GET_DATA_READY_STATUS)) {
      ESP_LOGE(TAG, "Failed to write data ready status command");
      this->mark_failed();
      return;
    }
    delay(20);  // per datasheet

    uint16_t raw_read_status;
    if (!this->read_data(raw_read_status)) {
      ESP_LOGE(TAG, "Failed to read data ready status");
      this->mark_failed();
      return;
    }

    uint32_t stop_measurement_delay = 0;
    // In order to query the device periodic measurement must be ceased
    if (raw_read_status) {
      ESP_LOGD(TAG, "Data is available; stopping periodic measurement");
      if (!this->write_command(SEN6X_CMD_STOP_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Failed to stop measurements");
        this->mark_failed();
        return;
      }
      // According to the sen6x datasheet the sensor will only respond to other commands after waiting 1400 ms after
      // issuing the stop_periodic_measurement command
      stop_measurement_delay = 1400;
    }
    this->set_timeout(stop_measurement_delay, [this]() {
      uint16_t raw_serial_number[16];  // 16 words pack 32 ASCII characters
      if (!this->get_register(SEN6X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 16, 20)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      this->unpack_uint16_to_char_(raw_serial_number, this->serial_number_);
      ESP_LOGV(TAG, "Serial number %.*s", (int) this->serial_number_.size(), this->serial_number_.data());

      uint16_t raw_product_name[16];  // 16 words pack 32 ASCII characters
      if (!this->get_register(SEN6X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
        ESP_LOGE(TAG, "Failed to read product name");
        this->error_code_ = PRODUCT_NAME_FAILED;
        this->mark_failed();
        return;
      }
      this->unpack_uint16_to_char_(raw_product_name, this->product_name_);
      ESP_LOGV(TAG, "Product name %.*s", (int) this->product_name_.size(), this->product_name_.data());

      // Determine sensor type from product name
      // NOTE: this is done via type parameter in YAML now, so this code is commented out
      // if (std::string_view(this->product_name_.data()) == "SEN62")
      //   this->sen6x_type_ = SEN62;
      // else if (std::string_view(this->product_name_.data()) == "SEN63C")
      //   this->sen6x_type_ = SEN63C;
      // else if (std::string_view(this->product_name_.data()) == "SEN65")
      //   this->sen6x_type_ = SEN65;
      // else if (std::string_view(this->product_name_.data()) == "SEN66")
      //   this->sen6x_type_ = SEN66;
      // else if (std::string_view(this->product_name_.data()) == "SEN68")
      //   this->sen6x_type_ = SEN68;
      // else if (std::string_view(this->product_name_.data()) == "SEN69C")
      //   this->sen6x_type_ = SEN69C;
      // ESP_LOGD(TAG, "Product name: %s", this->product_name_.data());

      if (this->voc_sensor_ && (this->sen6x_type_ == SEN62 || this->sen6x_type_ == SEN63C)) {
        ESP_LOGE(TAG, "VOC is not available on SEN62 and SEN63C");
        this->voc_sensor_ = nullptr;  // mark as not used
      }
      if (this->nox_sensor_ && (this->sen6x_type_ == SEN62 || this->sen6x_type_ == SEN63C)) {
        ESP_LOGE(TAG, "NOx is not available on SEN62 and SEN63C");
        this->nox_sensor_ = nullptr;  // mark as not used
      }
      if (this->co2_sensor_ && this->sen6x_type_ != SEN63C && this->sen6x_type_ != SEN66 &&
          this->sen6x_type_ != SEN69C) {
        ESP_LOGE(TAG, "CO2 requires SEN63C, SEN66 or SEN69C");
        this->co2_sensor_ = nullptr;  // mark as not used
      }
      if (this->hcho_sensor_ && this->sen6x_type_ != SEN68 && this->sen6x_type_ != SEN69C) {
        ESP_LOGE(TAG, "HCHO requires SEN68 or SEN69C");
        this->hcho_sensor_ = nullptr;  // mark as not used
      }

      uint16_t raw_firmware_version[1];
      if (!this->get_register(SEN6X_CMD_GET_FIRMWARE_VERSION, raw_firmware_version, 1, 20)) {
        ESP_LOGE(TAG, "Failed to read firmware version");
        this->error_code_ = FIRMWARE_FAILED;
        this->mark_failed();
        return;
      }
      this->unpack_uint16_to_char_(raw_firmware_version, this->firmware_version_);
      ESP_LOGV(TAG, "Firmware version %d.%d", this->firmware_version_[1], this->firmware_version_[0]);

      if (this->voc_sensor_ && this->store_baseline_) {
        uint32_t combined_serial =
            encode_uint24(this->serial_number_[0], this->serial_number_[1], this->serial_number_[2]);
        // Hash with compilation time and serial number
        // This ensures the baseline storage is cleared after OTA
        // Serial numbers are unique to each sensor, so mulitple sensors can be used without conflict
        uint32_t hash = fnv1_hash(App.get_compilation_time() + std::to_string(combined_serial));
        this->pref_ = global_preferences->make_preference<Sen6xBaselines>(hash, true);

        if (this->pref_.load(&this->voc_baselines_storage_)) {
          ESP_LOGI(TAG, "Loaded VOC baseline state0: 0x%04" PRIX32 ", state1: 0x%04" PRIX32,
                   this->voc_baselines_storage_.state0, this->voc_baselines_storage_.state1);
        }

        // Initialize storage timestamp
        this->seconds_since_last_store_ = 0;

        if (this->voc_baselines_storage_.state0 > 0 && this->voc_baselines_storage_.state1 > 0) {
          ESP_LOGI(TAG, "Setting VOC baseline from save state0: 0x%04" PRIX32 ", state1: 0x%04" PRIX32,
                   this->voc_baselines_storage_.state0, this->voc_baselines_storage_.state1);
          uint16_t states[4];

          states[0] = this->voc_baselines_storage_.state0 >> 16;
          states[1] = this->voc_baselines_storage_.state0 & 0xFFFF;
          states[2] = this->voc_baselines_storage_.state1 >> 16;
          states[3] = this->voc_baselines_storage_.state1 & 0xFFFF;

          if (!this->write_command(SEN6X_CMD_VOC_ALGORITHM_STATE, states, 4)) {
            ESP_LOGE(TAG, "Failed to set VOC baseline from saved state");
          }
        }
      }
      if (this->voc_tuning_params_.has_value()) {
        this->write_tuning_parameters_(SEN6X_CMD_VOC_ALGORITHM_TUNING, this->voc_tuning_params_.value());
        delay(20);
      }
      if (this->nox_tuning_params_.has_value()) {
        this->write_tuning_parameters_(SEN6X_CMD_NOX_ALGORITHM_TUNING, this->nox_tuning_params_.value());
        delay(20);
      }
      if (this->temperature_compensation_.has_value()) {
        this->write_temperature_compensation_(this->temperature_compensation_.value());
        delay(20);
      }
      if (this->pressure_compensation_.has_value()) {
        this->write_pressure_compensation_(this->pressure_compensation_.value());
        delay(20);
      }
      if (this->altitude_compensation_.has_value()) {
        this->write_altitude_compensation_(this->altitude_compensation_.value());
        delay(20);
      }

      // Finally start sensor measurements
      auto cmd = SEN6X_CMD_START_MEASUREMENTS;

      if (!this->write_command(cmd)) {
        ESP_LOGE(TAG, "Error starting continuous measurements");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
      this->initialized_ = true;
    });
  });
}

void SEN6XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN6X:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement initialization failed");
        break;
      case SERIAL_NUMBER_IDENTIFICATION_FAILED:
        ESP_LOGW(TAG, "Unable to read serial ID");
        break;
      case PRODUCT_NAME_FAILED:
        ESP_LOGW(TAG, "Unable to read product name");
        break;
      case FIRMWARE_FAILED:
        ESP_LOGW(TAG, "Unable to read firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error");
        break;
    }
  }
  ESP_LOGCONFIG(TAG,
                "  Product name: %s\n"
                "  Firmware version: %d.%d\n"
                "  Serial number %s",
                this->product_name_.data(), this->firmware_version_[1], this->firmware_version_[0],
                this->serial_number_.data());
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "PM  1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM  2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM  4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);    // SEN65, SEN66, SEN68 and SEN69C only
  LOG_SENSOR("  ", "NOx", this->nox_sensor_);    // SEN65, SEN66, SEN68 and SEN69C only
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);    // SEN63C, SEN66 and SEN69C only
  LOG_SENSOR("  ", "HCHO", this->hcho_sensor_);  // SEN68 and SEN69C only
}

void SEN6XComponent::update() {
  if (!this->initialized_) {
    return;
  }

  if (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN68 ||
      this->sen6x_type_ == SEN69C) {
    // Store baselines after defined interval or if the difference between current and stored baseline becomes too
    // much
    if (this->store_baseline_ && this->seconds_since_last_store_ > SHORTEST_BASELINE_STORE_INTERVAL) {
      if (this->write_command(SEN6X_CMD_VOC_ALGORITHM_STATE)) {
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
                ESP_LOGI(TAG, "Stored VOC baseline state0: 0x%04" PRIX32 ", state1: 0x%04" PRIX32,
                         this->voc_baselines_storage_.state0, this->voc_baselines_storage_.state1);
              } else {
                ESP_LOGW(TAG, "Could not store VOC baselines");
              }
            }
          }
        });
      }
    }
  }

  uint16_t cmd;
  switch (this->sen6x_type_) {
    case SEN62:
      cmd = SEN62_CMD_READ_MEASUREMENT;
      break;
    case SEN63C:
      cmd = SEN63C_CMD_READ_MEASUREMENT;
      break;
    case SEN65:
      cmd = SEN65_CMD_READ_MEASUREMENT;
      break;
    case SEN66:
      cmd = SEN66_CMD_READ_MEASUREMENT;
      break;
    case SEN68:
      cmd = SEN68_CMD_READ_MEASUREMENT;
      break;
    case SEN69C:
      cmd = SEN69C_CMD_READ_MEASUREMENT;
      break;
    default:
      ESP_LOGE(TAG, "Unknown SEN6X type");
      return;
  }
  if (!this->write_command(cmd)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "Write error: read measurement (%d)", this->last_error_);
    return;
  }
  this->set_timeout(20, [this]() {
    uint16_t measurements[10];  // max size for SEN69C
    uint16_t num_measurements;
    switch (this->sen6x_type_) {
      case SEN62:
        num_measurements = 6;
        break;
      case SEN63C:
        num_measurements = 7;
        break;
      case SEN65:
        num_measurements = 8;
        break;
      case SEN66:
      case SEN68:
        num_measurements = 9;
        break;
      case SEN69C:
        num_measurements = 10;
        break;
      default:
        ESP_LOGE(TAG, "Unknown SEN6X type");
        return;
    }

    if (!this->read_data(measurements, num_measurements)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "Read data error (%d)", this->last_error_);
      return;
    }

    ESP_LOGVV(TAG, "pm_1_0 = 0x%.4x", measurements[0]);
    float pm_1_0 = measurements[0] == UINT16_MAX ? NAN : measurements[0] / 10.0f;

    ESP_LOGVV(TAG, "pm_2_5 = 0x%.4x", measurements[1]);
    float pm_2_5 = measurements[1] == UINT16_MAX ? NAN : measurements[1] / 10.0f;

    ESP_LOGVV(TAG, "pm_4_0 = 0x%.4x", measurements[2]);
    float pm_4_0 = measurements[2] == UINT16_MAX ? NAN : measurements[2] / 10.0f;

    ESP_LOGVV(TAG, "pm_10_0 = 0x%.4x", measurements[3]);
    float pm_10_0 = measurements[3] == UINT16_MAX ? NAN : measurements[3] / 10.0f;

    ESP_LOGVV(TAG, "humidity = 0x%.4x", measurements[4]);
    float humidity = measurements[4] == INT16_MAX ? NAN : static_cast<int16_t>(measurements[4]) / 100.0f;

    ESP_LOGVV(TAG, "temperature = 0x%.4x", measurements[5]);
    float temperature = measurements[5] == INT16_MAX ? NAN : static_cast<int16_t>(measurements[5]) / 200.0f;

    float co2{NAN};
    float voc{NAN};
    float nox{NAN};
    float hcho{NAN};

    if (this->sen6x_type_ == SEN63C) {
      ESP_LOGVV(TAG, "co2 = 0x%.4x", measurements[6]);
      co2 = measurements[6] == UINT16_MAX ? NAN : measurements[6];
    }

    if (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN68 ||
        this->sen6x_type_ == SEN69C) {
      ESP_LOGVV(TAG, "voc = 0x%.4x", measurements[6]);
      int16_t voc_idx = static_cast<int16_t>(measurements[6]);
      voc = (voc_idx < SEN6X_MIN_INDEX_VALUE || voc_idx > SEN6X_MAX_INDEX_VALUE) ? NAN
                                                                                 : static_cast<float>(voc_idx) / 10.0f;

      ESP_LOGVV(TAG, "nox = 0x%.4x", measurements[7]);
      int16_t nox_idx = static_cast<int16_t>(measurements[7]);
      nox = (nox_idx < SEN6X_MIN_INDEX_VALUE || nox_idx > SEN6X_MAX_INDEX_VALUE) ? NAN
                                                                                 : static_cast<float>(nox_idx) / 10.0f;
    }

    if (this->sen6x_type_ == SEN66) {
      ESP_LOGVV(TAG, "co2 = 0x%.4x", measurements[8]);
      co2 = measurements[8] == UINT16_MAX ? NAN : measurements[8];
    }

    if (this->sen6x_type_ == SEN68 || this->sen6x_type_ == SEN69C) {
      ESP_LOGVV(TAG, "hcho = 0x%.4x", measurements[8]);
      hcho = measurements[8] == UINT16_MAX ? NAN : measurements[8] / 10.0f;
    }

    if (this->sen6x_type_ == SEN69C) {
      ESP_LOGVV(TAG, "co2 = 0x%.4x", measurements[9]);
      co2 = measurements[9] == UINT16_MAX ? NAN : measurements[9];
    }

    if (this->pm_1_0_sensor_ != nullptr) {
      this->pm_1_0_sensor_->publish_state(pm_1_0);
    }
    if (this->pm_2_5_sensor_ != nullptr) {
      this->pm_2_5_sensor_->publish_state(pm_2_5);
    }
    if (this->pm_4_0_sensor_ != nullptr) {
      this->pm_4_0_sensor_->publish_state(pm_4_0);
    }
    if (this->pm_10_0_sensor_ != nullptr) {
      this->pm_10_0_sensor_->publish_state(pm_10_0);
    }
    if (this->temperature_sensor_ != nullptr) {
      this->temperature_sensor_->publish_state(temperature);
    }
    if (this->humidity_sensor_ != nullptr) {
      this->humidity_sensor_->publish_state(humidity);
    }
    if (this->voc_sensor_ != nullptr) {
      this->voc_sensor_->publish_state(voc);
    }
    if (this->nox_sensor_ != nullptr) {
      this->nox_sensor_->publish_state(nox);
    }
    if (this->co2_sensor_ != nullptr) {
      this->co2_sensor_->publish_state(co2);
    }
    if (this->hcho_sensor_ != nullptr) {
      this->hcho_sensor_->publish_state(hcho);
    }
    this->status_clear_warning();
  });
}

bool SEN6XComponent::write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning) {
  uint16_t params[6];
  params[0] = tuning.index_offset;
  params[1] = tuning.learning_time_offset_hours;
  params[2] = tuning.learning_time_gain_hours;
  params[3] = tuning.gating_max_duration_minutes;
  params[4] = tuning.std_initial;
  params[5] = tuning.gain_factor;
  auto result = write_command(i2c_command, params, 6);
  if (!result) {
    ESP_LOGE(TAG, "Set tuning parameters failed (command=%0xX, err=%d)", i2c_command, this->last_error_);
  }
  return result;
}

bool SEN6XComponent::write_temperature_compensation_(const TemperatureCompensation &compensation) {
  uint16_t params[4];
  params[0] = compensation.offset;
  params[1] = compensation.normalized_offset_slope;
  params[2] = compensation.time_constant;
  params[3] = compensation.slot;
  if (!write_command(SEN6X_CMD_TEMPERATURE_COMPENSATION, params, 4)) {
    ESP_LOGE(TAG, "Set temperature_compensation failed (%d)", this->last_error_);
    return false;
  }
  return true;
}

bool SEN6XComponent::write_pressure_compensation_(const uint16_t pressure) {
  uint16_t params[1];
  params[0] = pressure;
  if (!write_command(SEN6X_CMD_PRESSURE_COMPENSATION, params, 1)) {
    ESP_LOGE(TAG, "Set pressure_compensation failed (%d)", this->last_error_);
    return false;
  }
  return true;
}

bool SEN6XComponent::write_altitude_compensation_(const uint16_t altitude) {
  uint16_t params[1];
  params[0] = altitude;
  if (!write_command(SEN6X_CMD_ALTITUDE_COMPENSATION, params, 1)) {
    ESP_LOGE(TAG, "Set altitude_compensation failed (%d)", this->last_error_);
    return false;
  }
  return true;
}

bool SEN6XComponent::start_fan_cleaning() {
  if (!write_command(SEN6X_CMD_START_CLEANING_FAN)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Start fan cleaning failed (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Fan auto clean started");
  }
  return true;
}

}  // namespace esphome::sen6x

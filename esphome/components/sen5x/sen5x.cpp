#include "sen5x.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen5x {

static const char *const TAG = "sen5x";

static const uint16_t SEN5X_CMD_READ_MEASUREMENT = 0x03C4;
static const uint16_t SEN62_CMD_READ_MEASUREMENT = 0x04A3;
static const uint16_t SEN63C_CMD_READ_MEASUREMENT = 0x0471;
static const uint16_t SEN65_CMD_READ_MEASUREMENT = 0x0446;
static const uint16_t SEN66_CMD_READ_MEASUREMENT = 0x0300;
static const uint16_t SEN68_CMD_READ_MEASUREMENT = 0x0467;
static const uint16_t SEN69C_CMD_READ_MEASUREMENT = 0x04B5;

static const uint16_t SEN6X_CMD_TEMPERATURE_ACCEL_PARAMETERS = 0x6100;
static const uint16_t SEN6X_CMD_PERFORM_FORCED_CO2_RECAL = 0x6707;
static const uint16_t SEN6X_CMD_CO2_SENSOR_AUTO_SELF_CAL = 0x6711;
static const uint16_t SEN6X_CMD_AMBIENT_PRESSURE = 0x6720;
static const uint16_t SEN6X_CMD_SENSOR_ALTITUDE = 0x6736;
static const uint16_t SEN6X_CMD_ACTIVATE_SHT_HEATER = 0x6765;
static const uint16_t SEN6X_CMD_READ_DEVICE_STATUS = 0xD206;
static const uint16_t SEN6X_CMD_READ_AND_CLEAR_DEVICE_STATUS = 0xD210;

static const uint16_t SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY = 0x0037;
static const uint16_t SEN5X_CMD_RHT_ACCELERATION_MODE = 0x60F7;
static const uint16_t SEN5X_CMD_AUTO_CLEANING_INTERVAL = 0x8004;

static const uint16_t CMD_RESET = 0xD304;
static const uint16_t CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t CMD_VOC_ALGORITHM_TUNING = 0x60D0;
static const uint16_t CMD_NOX_ALGORITHM_TUNING = 0x60E1;
static const uint16_t CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t CMD_GET_PRODUCT_NAME = 0xD014;
static const uint16_t CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t CMD_GET_FIRMWARE_VERSION = 0xD100;

static const int8_t SEN5X_INDEX_SCALE_FACTOR = 10;                            // used for VOC and NOx index values
static const int8_t SEN5X_MIN_INDEX_VALUE = 1 * SEN5X_INDEX_SCALE_FACTOR;     // must be adjusted by the scale factor
static const int16_t SEN5X_MAX_INDEX_VALUE = 500 * SEN5X_INDEX_SCALE_FACTOR;  // must be adjusted by the scale factor

static inline const char *model_to_str(Sen5xType model) {
  switch (model) {
    case SEN50:
      return "SEN50";
    case SEN54:
      return "SEN54";
    case SEN55:
      return "SEN55";
    case SEN62:
      return "SEN62";
    case SEN63C:
      return "SEN63C";
    case SEN65:
      return "SEN65";
    case SEN66:
      return "SEN66";
    case SEN68:
      return "SEN68";
    case SEN69C:
      return "SEN69C";
    default:
      return "UNKNOWN MODEL";
  }
}

static const LogString *rht_accel_mode_to_string(RhtAccelerationMode mode) {
  switch (mode) {
    case LOW_ACCELERATION:
      return LOG_STR("LOW");
    case MEDIUM_ACCELERATION:
      return LOG_STR("MEDIUM");
    case HIGH_ACCELERATION:
      return LOG_STR("HIGH");
    default:
      return LOG_STR("UNKNOWN");
  }
}

static inline std::string convert_to_string(uint16_t array[], uint8_t length) {
  for (int i = 0; i < length; i++) {
    array[i] = convert_big_endian(array[i]);
  }
  std::string new_string = reinterpret_cast<const char *>(array);
  return new_string;
}

void SEN5XComponent::setup() { this->internal_setup_(SEN5X_SM_START); }

void SEN5XComponent::internal_setup_(Sen5xSetupStates state) {
  uint16_t string_number[16] = {0};
  switch (state) {
    case SEN5X_SM_START:
      // the sensor needs 100 ms after power up before i2c bus communication can be established
      this->set_timeout(100, [this]() { this->internal_setup_(SEN5X_SM_START_1); });
      break;
    case SEN5X_SM_START_1:
      // Check if measurement is ready before reading the value
      if (!this->write_command(CMD_GET_DATA_READY_STATUS)) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
      this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_START_2); });
      break;
    case SEN5X_SM_START_2:
      uint16_t raw_read_status;
      if (!this->read_data(raw_read_status)) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
      // In order to query the device periodic measurement must be ceased
      if (raw_read_status) {
        ESP_LOGV(TAG, "Stopping periodic measurement");
        if (!this->stop_measurements_()) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(1400, [this]() { this->internal_setup_(SEN5X_SM_GET_SN); });
      } else {
        this->internal_setup_(SEN5X_SM_GET_SN);
      }
      break;
    case SEN5X_SM_GET_SN:
      if (!this->get_register(CMD_GET_SERIAL_NUMBER, string_number, 16, 20)) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
      this->serial_number_ = convert_to_string(string_number, 16);
      ESP_LOGV(TAG, "Serial number %s", this->serial_number_.c_str());
      this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_GET_PN); });
      break;
    case SEN5X_SM_GET_PN:
      if (!this->get_register(CMD_GET_PRODUCT_NAME, string_number, 16, 20)) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        this->mark_failed(LOG_STR("Product Name failed"));
        return;
      }
      this->product_name_ = convert_to_string(string_number, 16);
      if (this->product_name_.empty()) {
        // Can't verify configuration model matches connected sensor
        ESP_LOGW(TAG, "Product Name is empty");
      } else {
        // product name and model must match
        if (this->product_name_ != model_to_str(this->model_.value())) {
          ESP_LOGE(TAG, "Product Name failed");
          this->mark_failed(LOG_STR("Product Name failed"));
          return;
        }
      }
      ESP_LOGV(TAG, "Product Name %s", this->product_name_.c_str());
      this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_GET_FW); });
      break;
    case SEN5X_SM_GET_FW:
      uint16_t firmware;
      if (!this->get_register(CMD_GET_FIRMWARE_VERSION, &firmware, 1, 20)) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
      if (this->is_sen6x_()) {
        this->firmware_minor_ = firmware & 0xFF;
        this->firmware_major_ = firmware >> 8;
        ESP_LOGV(TAG, "Firmware version %u.%u", this->firmware_major_, this->firmware_minor_);
      } else {
        this->firmware_major_ = firmware >> 8;
        this->firmware_minor_ = 0xFF;  // not defined
        ESP_LOGV(TAG, "Firmware version %u", this->firmware_major_);
      }
      this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_VOCB); });
      break;
    case SEN5X_SM_SET_VOCB:
      if (this->store_baseline_) {
        // Hash with config hash, version, and serial number, ensures the baseline storage is cleared after OTA
        // Serial numbers are unique to each sensor, so multiple sensors can be used without conflict
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 1, 0)
        uint32_t hash = fnv1a_hash_extend(App.get_config_version_hash(), this->serial_number_);
#else
        uint32_t hash = fnv1_hash(App.get_compilation_time_ref() + this->serial_number_);
#endif
        this->pref_ = global_preferences->make_preference<uint16_t[4]>(hash, true);
        uint16_t state[4];
        this->last_store_time_ = App.get_loop_component_start_time();
        // ignore byte order, we are good as long as we are consistent loading and saving
        if (this->pref_.load(&state)) {
          if (this->write_command(CMD_VOC_ALGORITHM_STATE, state, 4)) {
            ESP_LOGV(TAG, "Loaded VOC baseline from flash");
            this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_ACI); });
            return;
          }
        }
      }
      this->internal_setup_(SEN5X_SM_SET_ACI);
      break;
    case SEN5X_SM_SET_ACI:
      if (this->auto_cleaning_interval_.has_value()) {
        if (!write_command(SEN5X_CMD_AUTO_CLEANING_INTERVAL, this->auto_cleaning_interval_.value())) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_ACCEL); });
      } else {
        this->internal_setup_(SEN5X_SM_SET_ACCEL);
      }
      break;
    case SEN5X_SM_SET_ACCEL:
      if (this->acceleration_mode_.has_value()) {
        if (!this->write_command(SEN5X_CMD_RHT_ACCELERATION_MODE, this->acceleration_mode_.value())) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_VOCT); });
      } else if (this->temperature_acceleration_.has_value()) {
        if (!this->write_temperature_acceleration_()) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_VOCT); });
      } else {
        this->internal_setup_(SEN5X_SM_SET_VOCT);
      }
      break;
    case SEN5X_SM_SET_VOCT:
      if (this->voc_tuning_params_.has_value()) {
        if (!this->write_tuning_parameters_(CMD_VOC_ALGORITHM_TUNING, this->voc_tuning_params_.value())) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_NOXT); });
      } else {
        this->internal_setup_(SEN5X_SM_SET_NOXT);
      }
      break;
    case SEN5X_SM_SET_NOXT:
      if (this->nox_tuning_params_.has_value()) {
        if (!this->write_tuning_parameters_(CMD_NOX_ALGORITHM_TUNING, this->nox_tuning_params_.value())) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_TP); });
      } else {
        this->internal_setup_(SEN5X_SM_SET_TP);
      }
      break;
    case SEN5X_SM_SET_TP:
      if (this->temperature_compensation_.has_value()) {
        if (!this->write_temperature_compensation_(this->temperature_compensation_.value())) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_CO2ASC); });
      } else {
        this->internal_setup_(SEN5X_SM_SET_CO2ASC);
      }
      break;
    case SEN5X_SM_SET_CO2ASC:
      if (this->auto_self_calibration_.has_value()) {
        if (!this->write_command(SEN6X_CMD_CO2_SENSOR_AUTO_SELF_CAL,
                                 this->auto_self_calibration_.value() ? 0x01 : 0x00)) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_SET_CO2AC); });
      } else {
        this->internal_setup_(SEN5X_SM_SET_CO2AC);
      }
      break;
    case SEN5X_SM_SET_CO2AC:
      if (this->altitude_compensation_.has_value()) {
        if (!this->write_command(SEN6X_CMD_SENSOR_ALTITUDE, this->altitude_compensation_.value())) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }
        this->set_timeout(20, [this]() { this->internal_setup_(SEN5X_SM_START_MEAS); });
      } else {
        this->internal_setup_(SEN5X_SM_START_MEAS);
      }
      break;
    case SEN5X_SM_START_MEAS:
      // Finally start sensor measurements
      if (!this->start_measurements_()) {
        this->mark_failed(LOG_STR("Measurement Start Failed"));
        return;
      }
      this->set_timeout(50, [this]() { this->internal_setup_(SEN5X_SM_DONE); });
      break;
    case SEN5X_SM_DONE:
      this->initialized_ = true;
      ESP_LOGD(TAG, "Initialized");
      break;
  }
}

void SEN5XComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SEN5X:\n"
                "  Initialized: %s\n"
                "  Model: %s\n"
                "  Update Interval: %ums\n"
                "  Serial number: %s\n",
                TRUEFALSE(this->initialized_), model_to_str(this->model_.value()), this->update_interval_,
                this->serial_number_.c_str());
  if (this->is_sen6x_()) {
    ESP_LOGCONFIG(TAG, "  Firmware version: %u.%u", this->firmware_major_, this->firmware_minor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Firmware version: %u", this->firmware_major_);
  }
  if (this->auto_cleaning_interval_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Auto cleaning interval: %" PRId32 "s", this->auto_cleaning_interval_.value());
  }
  if (this->acceleration_mode_.has_value()) {
    ESP_LOGCONFIG(TAG, "  RH/T acceleration mode: %s",
                  LOG_STR_ARG(rht_accel_mode_to_string(this->acceleration_mode_.value())));
  }
  if (this->store_baseline_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Store Baseline: %s", TRUEFALSE(this->store_baseline_.value()));
  }
  LOG_SENSOR("  ", "PM  1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM  2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM  4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);
  LOG_SENSOR("  ", "NOx", this->nox_sensor_);
  LOG_SENSOR("  ", "CO₂", this->co2_sensor_);
  if (this->co2_sensor_ != nullptr) {
    if (this->auto_self_calibration_.has_value()) {
      ESP_LOGCONFIG(TAG, "    Automatic self calibration: %s", ONOFF(this->auto_self_calibration_.value()));
    }
    if (this->ambient_pressure_compensation_source_ != nullptr) {
      ESP_LOGCONFIG(TAG, "    Ambient Pressure Compensation Source: %s",
                    this->ambient_pressure_compensation_source_->get_name().c_str());
    } else if (this->altitude_compensation_.has_value()) {
      ESP_LOGCONFIG(TAG, "    Altitude Compensation: %d", this->altitude_compensation_.value());
    }
  }
  LOG_SENSOR("  ", "HCHO", this->hcho_sensor_);
}

void SEN5XComponent::update() {
  if (!this->initialized_ || !this->running_ || this->busy_)
    return;
  uint16_t cmd;
  uint8_t length;
  switch (this->model_.value()) {
    case SEN50:
      cmd = SEN5X_CMD_READ_MEASUREMENT;
      length = 4;
      break;
    case SEN54:
      cmd = SEN5X_CMD_READ_MEASUREMENT;
      length = 7;
      break;
    case SEN55:
      cmd = SEN5X_CMD_READ_MEASUREMENT;
      length = 8;
      break;
    case SEN62:
      cmd = SEN62_CMD_READ_MEASUREMENT;
      length = 6;
      break;
    case SEN63C:
      cmd = SEN63C_CMD_READ_MEASUREMENT;
      length = 7;
      break;
    case SEN65:
      cmd = SEN65_CMD_READ_MEASUREMENT;
      length = 8;
      break;
    case SEN66:
      cmd = SEN66_CMD_READ_MEASUREMENT;
      length = 9;
      break;
    case SEN68:
      cmd = SEN68_CMD_READ_MEASUREMENT;
      length = 9;
      break;
    case SEN69C:
      cmd = SEN69C_CMD_READ_MEASUREMENT;
      length = 10;
      break;
    default:
      ESP_LOGE(TAG, "Unsupported model");
      this->status_set_warning();
      return;
  }
  if (!this->write_command(cmd)) {
    this->status_set_warning();
    ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  this->set_timeout(20, [this, length]() {
    uint16_t measurements[10];
    if (!this->read_data(measurements, length)) {
      ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
      this->status_set_warning();
      return;
    }
    if (this->pm_1_0_sensor_ != nullptr) {
      ESP_LOGV(TAG, "pm_1_0 = 0x%.4x", measurements[0]);
      float pm_1_0 = measurements[0] == UINT16_MAX ? NAN : measurements[0] / 10.0f;
      this->pm_1_0_sensor_->publish_state(pm_1_0);
    }
    if (this->pm_2_5_sensor_ != nullptr) {
      ESP_LOGV(TAG, "pm_2_5 = 0x%.4x", measurements[1]);
      float pm_2_5 = measurements[1] == UINT16_MAX ? NAN : measurements[1] / 10.0f;
      this->pm_2_5_sensor_->publish_state(pm_2_5);
    }
    if (this->pm_4_0_sensor_ != nullptr) {
      ESP_LOGV(TAG, "pm_4_0 = 0x%.4x", measurements[2]);
      float pm_4_0 = measurements[2] == UINT16_MAX ? NAN : measurements[2] / 10.0f;
      this->pm_4_0_sensor_->publish_state(pm_4_0);
    }
    if (this->pm_10_0_sensor_ != nullptr) {
      ESP_LOGV(TAG, "pm_10_0 = 0x%.4x", measurements[3]);
      float pm_10_0 = measurements[3] == UINT16_MAX ? NAN : measurements[3] / 10.0f;
      this->pm_10_0_sensor_->publish_state(pm_10_0);
    }
    if (this->temperature_sensor_ != nullptr) {
      float temperature = static_cast<int16_t>(measurements[5]) / 200.0f;
      if ((this->is_sen6x_() && measurements[5] == INT16_MAX) ||
          (!this->is_sen6x_() && measurements[5] == UINT16_MAX)) {
        temperature = NAN;
      }
      ESP_LOGV(TAG, "temperature = 0x%.4x", measurements[5]);
      this->temperature_sensor_->publish_state(temperature);
    }
    if (this->humidity_sensor_ != nullptr) {
      float humidity = static_cast<int16_t>(measurements[4]) / 100.0f;
      if ((this->is_sen6x_() && measurements[4] == INT16_MAX) ||
          (!this->is_sen6x_() && measurements[4] == UINT16_MAX)) {
        humidity = NAN;
      }
      ESP_LOGV(TAG, "humidity = 0x%.4x", measurements[4]);
      this->humidity_sensor_->publish_state(humidity);
    }
    if (this->voc_sensor_ != nullptr) {
      ESP_LOGV(TAG, "voc = 0x%.4x", measurements[6]);
      int16_t voc_idx = static_cast<int16_t>(measurements[6]);
      float voc = (voc_idx < SEN5X_MIN_INDEX_VALUE || voc_idx > SEN5X_MAX_INDEX_VALUE) ? NAN : voc_idx / 10.0f;
      this->voc_sensor_->publish_state(voc);
    }
    if (this->nox_sensor_ != nullptr) {
      ESP_LOGV(TAG, "nox = 0x%.4x", measurements[7]);
      int16_t nox_idx = static_cast<int16_t>(measurements[7]);
      float nox = (nox_idx < SEN5X_MIN_INDEX_VALUE || nox_idx > SEN5X_MAX_INDEX_VALUE) ? NAN : nox_idx / 10.0f;
      this->nox_sensor_->publish_state(nox);
    }
    if (this->co2_sensor_ != nullptr) {
      if (this->model_.value() == SEN63C || this->model_.value() == SEN69C) {
        int16_t value = static_cast<int16_t>(measurements[6]);  // SEN63C reports as signed
        if (this->model_.value() == SEN69C) {
          value = static_cast<int16_t>(measurements[9]);  // SEN69C reports as signed
        }
        ESP_LOGV(TAG, "co2 = 0x%.4x", value);
        float co2_1 = value == INT16_MAX ? NAN : value / 1.0f;
        this->co2_sensor_->publish_state(co2_1);
      } else {
        ESP_LOGV(TAG, "co2 = 0x%.4x", measurements[8]);  // SEN66 reports as unsigned
        float co2_2 = measurements[8] == UINT16_MAX ? NAN : measurements[8] / 1.0f;
        this->co2_sensor_->publish_state(co2_2);
      }
    }
    if (this->hcho_sensor_ != nullptr) {
      ESP_LOGV(TAG, "HCHO = 0x%.4x", measurements[8]);
      float hcho = measurements[8] == UINT16_MAX ? NAN : measurements[8] / 10.0f;
      this->hcho_sensor_->publish_state(hcho);
    }
    uint32_t timeout = 0;
    if (this->ambient_pressure_compensation_source_ != nullptr) {
      float pressure = this->ambient_pressure_compensation_source_->state;
      if (!std::isnan(pressure)) {
        uint16_t new_ambient_pressure = static_cast<uint16_t>(pressure);
        if (!write_ambient_pressure_compensation_(new_ambient_pressure)) {
          this->status_set_warning();
          return;
        }
        timeout = 20;
      }
    }
    this->set_timeout(timeout, [this]() {
      if (!this->store_baseline_.has_value() || !this->store_baseline_.value() ||
          (App.get_loop_component_start_time() - this->last_store_time_) < SHORTEST_BASELINE_STORE_INTERVAL) {
        this->status_clear_warning();
      } else {
        this->last_store_time_ = App.get_loop_component_start_time();
        if (!this->write_command(CMD_VOC_ALGORITHM_STATE)) {
          this->status_set_warning();
          ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
        } else {
          this->set_timeout(20, [this]() {
            uint16_t state[4];
            if (!this->read_data(state, 4)) {
              this->status_set_warning();
              ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
            } else {
              if (this->pref_.save(&state)) {
                ESP_LOGD(TAG, "Saved VOC baseline to flash");
              }
              this->status_clear_warning();
            }
          });
        }
      }
    });
  });
}

bool SEN5XComponent::start_measurements_() {
  uint16_t cmd;
  if (is_sen6x_() || this->pm_1_0_sensor_ || this->pm_2_5_sensor_ || this->pm_4_0_sensor_ || this->pm_10_0_sensor_) {
    cmd = CMD_START_MEASUREMENTS;
  } else {
    cmd = SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY;
  }
  auto result = this->write_command(cmd);
  if (!result) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  } else {
    this->running_ = true;
    ESP_LOGD(TAG, "Measurements Enabled");
  }
  return result;
}

bool SEN5XComponent::stop_measurements_() {
  auto result = this->write_command(CMD_STOP_MEASUREMENTS);
  if (!result) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  } else {
    this->running_ = false;
    if (this->initialized_) {
      ESP_LOGD(TAG, "Measurements Stopped");
    }
  }
  return result;
}

bool SEN5XComponent::write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning) {
  uint16_t params[6];
  params[0] = tuning.index_offset;
  params[1] = tuning.learning_time_offset_hours;
  params[2] = tuning.learning_time_gain_hours;
  params[3] = tuning.gating_max_duration_minutes;
  params[4] = tuning.std_initial;
  params[5] = tuning.gain_factor;
  auto result = this->write_command(i2c_command, params, 6);
  if (!result) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  return result;
}

bool SEN5XComponent::write_temperature_compensation_(const TemperatureCompensation &compensation) {
  uint16_t params[4];
  params[0] = static_cast<uint16_t>(compensation.offset);
  params[1] = static_cast<uint16_t>(compensation.normalized_offset_slope);
  params[2] = compensation.time_constant;
  params[3] = compensation.slot;
  uint8_t write_cnt = this->is_sen6x_() ? 4 : 3;
  auto result = this->write_command(CMD_TEMPERATURE_COMPENSATION, params, write_cnt);
  if (!result) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  return result;
}

bool SEN5XComponent::write_temperature_acceleration_() {
  uint16_t params[4];
  if (this->temperature_acceleration_.has_value()) {
    auto accel_param = this->temperature_acceleration_.value();
    params[0] = accel_param.k;
    params[1] = accel_param.p;
    params[2] = accel_param.t1;
    params[3] = accel_param.t2;
    auto result = this->write_command(SEN6X_CMD_TEMPERATURE_ACCEL_PARAMETERS, params, 4);
    if (!result) {
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      return false;
    }
  }
  return true;
}

bool SEN5XComponent::write_ambient_pressure_compensation_(uint16_t pressure_in_hpa) {
  if (this->ambient_pressure_compensation_ != pressure_in_hpa) {
    this->ambient_pressure_compensation_ = pressure_in_hpa;
    if (!this->write_command(SEN6X_CMD_AMBIENT_PRESSURE, pressure_in_hpa)) {
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      return false;
    }
    ESP_LOGD(TAG, "Ambient Pressure Compensation updated, pres=%d hPa", pressure_in_hpa);
  }
  return true;
}

bool SEN5XComponent::is_sen6x_() {
  return this->model_.value() != SEN50 && this->model_.value() != SEN54 && this->model_.value() != SEN55;
}

void SEN5XComponent::set_ambient_pressure_compensation(uint16_t pressure_in_hpa) {
  if (this->model_.value() == SEN63C || this->model_.value() == SEN66 || this->model_.value() == SEN69C) {
    if (!this->initialized_) {
      return;
    }
    if (this->busy_) {
      ESP_LOGW(TAG, "Ambient Pressure Compensation aborted, sensor is busy");
      return;
    }
    this->busy_ = true;
    if (!write_ambient_pressure_compensation_(pressure_in_hpa)) {
      ESP_LOGE(TAG, "Ambient Pressure Compensation failed");
      this->busy_ = false;
      return;
    }
    this->set_timeout(20, [this]() { this->busy_ = false; });
  } else {
    ESP_LOGE(TAG, "Set Ambient Pressure Compensation is not supported");
  }
}

void SEN5XComponent::start_fan_cleaning() {
  if (this->busy_) {
    ESP_LOGW(TAG, "Fan Cleaning aborted, sensor is busy");
    return;
  }
  ESP_LOGD(TAG, "Fan Cleaning started (12s)");
  this->busy_ = true;  // prevent actions from stomping on each other
  // measurements must be stopped for SEN6X and must be running for SEN5X
  if (this->is_sen6x_()) {
    if (!this->stop_measurements_()) {
      ESP_LOGE(TAG, "Fan Cleaning failed");
      this->busy_ = false;
      return;
    }
  }
  this->set_timeout(1400, [this]() {
    if (!this->write_command(CMD_START_CLEANING_FAN)) {
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      if (!this->running_) {
        this->start_measurements_();
      }
      ESP_LOGE(TAG, "Fan Cleaning failed");
      this->set_timeout(50, [this]() { this->busy_ = false; });
    } else {
      this->set_timeout(10000, [this]() {
        if (!this->running_) {
          if (!this->start_measurements_()) {
            ESP_LOGE(TAG, "Fan Cleaning failed");
            this->busy_ = false;
            return;
          }
        }
        ESP_LOGD(TAG, "Fan Cleaning finished");
        this->set_timeout(50, [this]() { this->busy_ = false; });
      });
    }
  });
}

void SEN5XComponent::activate_heater() {
  if (this->is_sen6x_()) {
    if (this->busy_) {
      ESP_LOGW(TAG, "Activate Heater aborted, sensor is busy");
      return;
    }
    ESP_LOGD(TAG, "Activate Heater started (22s)");
    this->busy_ = true;  // prevent actions from stomping on each other
    if (!this->stop_measurements_()) {
      ESP_LOGE(TAG, "Activate Heater failed");
      this->busy_ = false;
      return;
    }
    this->set_timeout(1400, [this]() {
      if (!this->write_command(SEN6X_CMD_ACTIVATE_SHT_HEATER)) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        this->start_measurements_();
        this->set_timeout(50, [this]() { this->busy_ = false; });
      } else {
        this->set_timeout(20000, [this]() {
          if (!this->start_measurements_()) {
            this->busy_ = false;
            ESP_LOGE(TAG, "Activate Heater failed");
          } else {
            ESP_LOGD(TAG, "Activate Heater finished");  // more than 10s after start
            this->set_timeout(50, [this]() { this->busy_ = false; });
          }
        });
      }
    });
  } else {
    ESP_LOGE(TAG, "Activate Heater is not supported");
  }
}

void SEN5XComponent::perform_forced_co2_recalibration(uint16_t co2) {
  if (this->model_.value() == SEN63C || this->model_.value() == SEN66 || this->model_.value() == SEN69C) {
    if (this->busy_) {
      ESP_LOGW(TAG, "Forced CO₂ Recalibration aborted, sensor is busy");
      return;
    }
    ESP_LOGD(TAG, "Forced CO₂ Recalibration started, co2=%d", co2);
    this->busy_ = true;  // prevent actions from stomping on each other
    if (!this->stop_measurements_()) {
      ESP_LOGE(TAG, "Forced CO₂ Recalibration failed");
      this->busy_ = false;
      return;
    }
    this->set_timeout(1400, [this, co2]() {
      if (!this->write_command(SEN6X_CMD_PERFORM_FORCED_CO2_RECAL, co2)) {
        this->start_measurements_();
        ESP_LOGE(TAG, "Forced CO₂ Recalibration failed");
        this->set_timeout(50, [this]() { this->busy_ = false; });
      } else {
        this->set_timeout(500, [this]() {
          uint16_t correction = 0;
          if (!this->read_data(correction)) {
            ESP_LOGE(TAG, "Forced CO₂ Recalibration failed");
          } else {
            if (correction == 0xFFFF) {
              ESP_LOGE(TAG, "Forced CO₂ Recalibration failed");
            } else {
              ESP_LOGD(TAG, "Forced CO₂ Recalibration finished, corr=%d", static_cast<int32_t>(correction) - 0x8000);
            }
          }
          if (!this->start_measurements_()) {
            ESP_LOGE(TAG, "Forced CO₂ Recalibration failed");
          }
          this->set_timeout(50, [this]() { this->busy_ = false; });
        });
      }
    });
  } else {
    ESP_LOGE(TAG, "Forced CO₂ Recalibration is not supported");
  }
}

void SEN5XComponent::set_temperature_compensation(float offset, float normalized_offset_slope, uint16_t time_constant,
                                                  uint8_t slot) {
  if (this->model_.value() != SEN50) {
    TemperatureCompensation compensation(offset, normalized_offset_slope, time_constant, slot);
    this->temperature_compensation_ = compensation;
    if (!this->initialized_) {
      return;
    }
    if (this->busy_) {
      ESP_LOGW(TAG, "Set Temperature Compensation aborted, sensor is busy");
      return;
    }
    ESP_LOGD(TAG, "Set Temperature Compensation, offset=%f, normalized_offset_slope=%f, time_constant=%d, slot=%d",
             offset, normalized_offset_slope, time_constant, slot);
    this->busy_ = true;  // prevent actions from stomping on each other
    if (!this->write_temperature_compensation_(compensation)) {
      ESP_LOGE(TAG, "Set Temperature Compensation failed");
    }
    this->set_timeout(50, [this]() { this->busy_ = false; });
  } else {
    ESP_LOGE(TAG, "Set Temperature Compensation is not supported");
  }
}

}  // namespace sen5x
}  // namespace esphome

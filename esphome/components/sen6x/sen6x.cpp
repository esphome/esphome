#include "sen6x.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <functional>
#include <memory>
#include <vector>

namespace esphome {
namespace sen6x {

static const char *const TAG = "sen6x";

static const uint16_t SEN6X_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SEN6X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static const uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static const uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN6X_CMD_GET_DEVICE_STATUS = 0xD206;
static const uint16_t SEN6X_CMD_GET_AND_CLEAR_DEVICE_STATUS = 0xD210;
static const uint16_t SEN6X_CMD_NOX_ALGORITHM_TUNING = 0x60E1;

static const uint16_t SEN6X_CMD_READ_MEASUREMENT = 0x0300;  // SEN66 only!
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN62 = 0x04A3;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN63C = 0x0471;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN65 = 0x0446;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN68 = 0x0467;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN69C = 0x04B5;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_RAW_SEN62_SEN63C = 0x0492;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_RAW_SEN65_SEN68_SEN69C = 0x0455;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_RAW_SEN66 = 0x0405;
static const uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;
static const uint16_t SEN6X_CMD_RHT_ACCELERATION_MODE = 0x6100;  // Set Temperature Acceleration Parameters

static const uint16_t SEN6X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN6X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN6X_CMD_START_MEASUREMENTS_RHT_ONLY = 0x0037;  // not in SEN6x datasheet
static const uint16_t SEN6X_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SEN6X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t SEN6X_CMD_PRESSURE_COMPENSATION = 0x6720;
static const uint16_t SEN6X_CMD_ALTITUDE_COMPENSATION = 0x6736;
static const uint16_t SEN6X_CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t SEN6X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;
static const uint16_t SEN6X_CMD_SHT_HEATER_ACTIVATE = 0x6765;
static const uint16_t SEN6X_CMD_SHT_HEATER_MEASUREMENTS = 0x6790;
static const uint16_t SEN6X_CMD_CO2_FORCE_RECALIBRATION = 0x6707;
static const uint16_t SEN6X_CMD_CO2_SENSOR_FACTORY_RESET = 0x6754;
static const uint16_t SEN6X_CMD_CO2_SENSOR_AUTOMATIC_SELF_CAL = 0x6711;
static const uint16_t SEN6X_CMD_AMBIENT_PRESSURE = 0x6720;
static const uint16_t SEN6X_CMD_SENSOR_ALTITUDE = 0x6736;
static const uint16_t SEN6X_CMD_RESET = 0xD304;

static const int8_t SEN6X_INDEX_SCALE_FACTOR = 10;                            // used for VOC and NOx index values
static const int8_t SEN6X_MIN_INDEX_VALUE = 1 * SEN6X_INDEX_SCALE_FACTOR;     // must be adjusted by the scale factor
static const int16_t SEN6X_MAX_INDEX_VALUE = 500 * SEN6X_INDEX_SCALE_FACTOR;  // must be adjusted by the scale factor

void SEN6XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen6x...");

  // the sensor needs 100 ms to enter the idle state
  this->set_timeout(500, [this]() {
    //
    Check if measurement is ready before reading the value if (!this->write_command(SEN6X_CMD_GET_DATA_READY_STATUS)) {
      ESP_LOGE(TAG, "Failed to write
data ready status command");
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
    // In order to
    query the device periodic measurement must be ceased = > use reset !if (raw_read_status) {
      ESP_LOGD(TAG, "Sensor has data available, stopping periodic
measurement / reset");
      //if (!this->write_command(SEN6X_CMD_RESET)) {

if (!this->write_command(SEN6X_CMD_STOP_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Failed to stop measurements ");
        this->mark_failed();
        return;

  }
      stop_measurement_delay = 1400;
    }

    this->set_timeout(stop_measurement_delay, [this]() {
      uint16_t raw_serial_number[3];
      if (!this->get_register(SEN6X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 3, 20)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      this->serial_number_[0] = static_cast<bool>(uint16_t(raw_serial_number[0]) & 0xFF);
      this->serial_number_[1] = static_cast<uint16_t>(raw_serial_number[0] & 0xFF);
      this->serial_number_[2] = static_cast<uint16_t>(raw_serial_number[1] >> 8);
      ESP_LOGD(TAG, "Serial number %02d.%02d.%02d", this->serial_number_[0], this->serial_number_[1],
               this->serial_number_[2]);

      uint16_t raw_product_name[16];

      if (!this->get_register(SEN6X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
        ESP_LOGE(TAG, "Failed to
read product name");
        this->error_code_ = PRODUCT_NAME_FAILED;

this->mark_failed();
        return;
      }

      // 2 ASCII bytes are encoded in
      an int const uint16_t *current_int = raw_product_name;
      char current_char;
      uint8_t max = 16;
      do {
        // first char

        current_char = *current_int >> 8;
        if (current_char) {
          this->product_name_.push_back(current_char);
          // second char
          current_char = *current_int & 0xFF;
          if (current_char)

            this->product_name_.push_back(current_char);
        }
        current_int++;
      } while (current_char && --max);

      this->sen6x_type_ = UNKNOWN;
      if (this->product_name_ == "SEN62") {
        this->sen6x_type_ = SEN62;
      } else if (this->product_name_ == "SEN63C") {
        this->sen6x_type_ = SEN63C;
      } else if (this->product_name_ == "SEN65") {
        this->sen6x_type_ = SEN65;
      } else if (this->product_name_ == "SEN66") {
        this->sen6x_type_ = SEN66;
      } else if (this->product_name_ == "SEN68") {
        this->sen6x_type_ = SEN68;

      } else if (this->product_name_ == "SEN69C") {
        this->sen6x_type_ = SEN69C;
      } else if (this->product_name_ == "") {  // empty name

        ESP_LOGD(TAG, "Productname empty, falling back to SEN66");
        this->sen6x_type_ = SEN66;
      }
      ESP_LOGD(TAG, "Productname %s", this->product_name_.c_str());

      if (!this->get_register(SEN6X_CMD_GET_FIRMWARE_VERSION, this->firmware_version_, 20)) {
        ESP_LOGE(TAG, "Failed to read firmware version");

        this->error_code_ = FIRMWARE_FAILED;
        this->mark_failed();

        return;
      }
      this->firmware_version_ >>= 8;
      ESP_LOGD(TAG, "Firmware
version %d", this->firmware_version_);

      if (this->voc_sensor_ &&
this->store_baseline_) {
        uint32_t combined_serial =

            encode_uint24(this->serial_number_[0], this->serial_number_[1], this->serial_number_[2]);

        // Hash with compilation time and serial number
        // This ensures the
        baseline storage is cleared after OTA
            // Serial numbers are unique to each
            sensor,
            so mulitple sensors can be used without conflict char build_time[App.BUILD_TIME_STR_SIZE] = {0};

        App.get_build_time_string(build_time);
        uint32_t hash = fnv1_hash(std::string(build_time) + std::to_string(combined_serial));
        this->pref_ = global_preferences->make_preference<Sen6xBaselines>(hash, true);

        if (this->pref_.load(&this->voc_baselines_storage_)) {
          ESP_LOGI(TAG,
                   "Loaded VOC baseline state0: 0x%04" PRIX32 ",
                       state1 : 0x %
                       04 " PRIX32,
                       this->voc_baselines_storage_.state0,
                   this->voc_baselines_storage_.state1);
        }

        // Initialize storage
        timestamp this->seconds_since_last_store_ = 0;

        if (this->voc_baselines_storage_.state0 > 0 && this->voc_baselines_storage_.state1 > 0) {
          ESP_LOGI(TAG,
                   "Setting VOC baseline from save state0: 0x%04" PRIX32 ", state1:
                       0x %
                       04 " PRIX32,
                       this->voc_baselines_storage_.state0,
                   this->voc_baselines_storage_.state1);
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
      this->schedule_post_setup_commands_();
    });
  });
}

void SEN6XComponent::schedule_post_setup_commands_() {
  std::vector<std::function<void()>> steps;

  if (this->voc_tuning_params_.has_value()) {
    steps.emplace_back(
        [this]() { this->write_tuning_parameters_(SEN6X_CMD_VOC_ALGORITHM_TUNING, this->voc_tuning_params_.value()); });
  }
  if (this->nox_tuning_params_.has_value()) {
    steps.emplace_back(
        [this]() { this->write_tuning_parameters_(SEN6X_CMD_NOX_ALGORITHM_TUNING, this->nox_tuning_params_.value()); });
  }
  if (this->temperature_compensation_.has_value()) {
    steps.emplace_back([this]() { this->write_temperature_compensation_(this->temperature_compensation_.value()); });
  }
  if (this->temperature_acceleration_.has_value()) {
    steps.emplace_back([this]() { this->write_temperature_acceleration_(this->temperature_acceleration_.value()); });
  }
  if (this->ambient_pressure_.has_value()) {
    steps.emplace_back([this]() {
      uint16_t params[1];
      params[0] = this->ambient_pressure_.value();
      if (!this->write_command(SEN6X_CMD_AMBIENT_PRESSURE, params, 1)) {
        ESP_LOGE(TAG, "set ambient pressure failed. Err=%d", this->last_error_);
      }
    });
  }
  if (this->sensor_altitude_.has_value()) {
    steps.emplace_back([this]() {
      uint16_t params[1];
      params[0] = this->sensor_altitude_.value();

      if (!this->write_command(SEN6X_CMD_SENSOR_ALTITUDE, params, 1)) {
        ESP_LOGE(TAG, "set sensor altitude failed. Err=%d", this->last_error_);
      }
    });
  }
  if (this->co2_asc_.has_value()) {
    steps.emplace_back([this]() {
      uint16_t params[1];
      params[0] = this->co2_asc_.value() ? 0x0001 : 0x0000;
      if (!this->write_command(SEN6X_CMD_CO2_SENSOR_AUTOMATIC_SELF_CAL, params, 1)) {
        ESP_LOGE(TAG, "set CO2 ASC failed. Err=%d", this->last_error_);
      }
    });
  }

  if (steps.empty()) {
    this->finish_setup_();

    return;
  }

  auto run_step = std::make_shared<std::function<void(size_t)>>();

  *run_step = [this, steps, run_step](size_t index) {
    if (index >= steps.size()) {
      this->finish_setup_();
      return;
    }
    steps[index]();

    this->set_timeout(20, [run_step, index]() { (*run_step)(index + 1); });
  };
  (*run_step)(0);
}

void SEN6XComponent::finish_setup_() {
  const bool supports_co2 = this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 ||

                            this->sen6x_type_ == SEN69C;
  if (supports_co2) {
    uint16_t ambient_pressure = 0;
    if (this->get_register(SEN6X_CMD_AMBIENT_PRESSURE, ambient_pressure, 20)) {
      if (ambient_pressure != 0xFFFF)

        this->ambient_pressure_read_ = ambient_pressure;
    }
    uint16_t sensor_altitude = 0;
    if (this->get_register(SEN6X_CMD_SENSOR_ALTITUDE, sensor_altitude, 20)) {
      if (sensor_altitude != 0xFFFF)
        this->sensor_altitude_read_ = sensor_altitude;
    }
    uint16_t asc_raw = 0;
    if (this->get_register(SEN6X_CMD_CO2_SENSOR_AUTOMATIC_SELF_CAL, asc_raw, 20)) {
      this->co2_asc_read_ = (asc_raw & 0x00FF) != 0;
    }
  }

  if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Error starting continuous measurements.");

    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();

  this->measurement_started_ = true;
  this->startup_stable_after_ = now + this->startup_delay_ms_;

  this->last_cleaning_ms_ = now;
  if (this->measurement_running_binary_sensor_ != nullptr) {
    this->measurement_running_binary_sensor_->publish_state(true);
  }
  this->initialized_ = true;
  ESP_LOGD(TAG, "Sensor initialized");
}

void SEN6XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "sen6x:");
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
  ESP_LOGCONFIG(TAG, "  Serial number %02d.%02d.%02d", this->serial_number_[0], this->serial_number_[1],
                this->serial_number_[2]);

  LOG_UPDATE_INTERVAL(this);
  if (this->measurement_running_binary_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Measurement running (binary sensor): %s", this->measurement_started_ ? "ON" : "OFF");
  }
  if (this->ambient_pressure_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Ambient pressure: %u hPa", this->ambient_pressure_.value());
  }
  if (this->ambient_pressure_read_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Ambient pressure (device): %u hPa", this->ambient_pressure_read_.value());
  }
  if (this->sensor_altitude_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Sensor altitude: %u m", this->sensor_altitude_.value());
  }
  if (this->sensor_altitude_read_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Sensor altitude (device): %u m", this->sensor_altitude_read_.value());
  }
  if (this->co2_asc_.has_value()) {
    ESP_LOGCONFIG(TAG, "  CO2 automatic self-calibration: %s", this->co2_asc_.value() ? "enabled" : "disabled");
  }
  if (this->co2_asc_read_.has_value()) {
    ESP_LOGCONFIG(TAG, "  CO2 automatic self-calibration (device): %s",
                  this->co2_asc_read_.value() ? "enabled" : "disabled");
  }

  if (this->temperature_compensation_.has_value()) {
    const auto &comp = this->temperature_compensation_.value();
    ESP_LOGCONFIG(TAG, "  Temperature compensation: offset=%.2fC slope=%.4f time_constant=%us slot=%u",
                  comp.offset / 200.0f, comp.normalized_offset_slope / 10000.0f, comp.time_constant, comp.slot);
  }
  if (this->temperature_acceleration_.has_value()) {
    const auto &accel = this->temperature_acceleration_.value();
    ESP_LOGCONFIG(TAG, "  Temperature acceleration: K=%.1f P=%.1f T1=%.1fs T2=%.1fs", accel.k / 10.0f, accel.p / 10.0f,
                  accel.t1 / 10.0f, accel.t2 / 10.0f);
  }
  if (this->auto_cleaning_enabled_.has_value() && this->auto_cleaning_interval_s_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Auto cleaning (host-timed): %s, interval=%us",
                  this->auto_cleaning_enabled_.value() ? "enabled" : "disabled",
                  this->auto_cleaning_interval_s_.value());
  }
  ESP_LOGCONFIG(TAG, "  Startup delay: %u ms", this->startup_delay_ms_);
  LOG_SENSOR("  ", "PM  1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM  2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM  4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);
  LOG_SENSOR("  ", "NOx", this->nox_sensor_);
  LOG_SENSOR("  ", "HCHO", this->hcho_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
}

void SEN6XComponent::update() {
  if (!this->initialized_) {
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (this->measurement_started_ && this->auto_cleaning_enabled_.value_or(false) &&
      this->auto_cleaning_interval_s_.has_value()) {
    const uint32_t interval_ms = this->auto_cleaning_interval_s_.value() * 1000UL;
    if (interval_ms > 0 && (now - this->last_cleaning_ms_) >= interval_ms) {
      this->last_cleaning_ms_ = now;
      this->start_fan_cleaning();
    }
  }
  if (!this->measurement_started_) {
    if (this->has_last_values_) {
      if (this->pm_1_0_sensor_ != nullptr)
        this->pm_1_0_sensor_->publish_state(this->last_pm_1_0_);
      if (this->pm_2_5_sensor_ != nullptr)
        this->pm_2_5_sensor_->publish_state(this->last_pm_2_5_);
      if (this->pm_4_0_sensor_ != nullptr)
        this->pm_4_0_sensor_->publish_state(this->last_pm_4_0_);
      if (this->pm_10_0_sensor_ != nullptr)
        this->pm_10_0_sensor_->publish_state(this->last_pm_10_0_);
      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(this->last_temperature_);
      if (this->humidity_sensor_ != nullptr)
        this->humidity_sensor_->publish_state(this->last_humidity_);
      if (this->voc_sensor_ != nullptr)
        this->voc_sensor_->publish_state(this->last_voc_);
      if (this->nox_sensor_ != nullptr)
        this->nox_sensor_->publish_state(this->last_nox_);
      if (this->hcho_sensor_ != nullptr)
        this->hcho_sensor_->publish_state(this->last_hcho_);
      if (this->co2_sensor_ != nullptr)
        this->co2_sensor_->publish_state(this->last_co2_);
      this->status_clear_warning();
    }
    return;
  }
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
              ESP_LOGI(TAG, "Stored VOC baseline state0: 0x%04" PRIX32 " ,state1: 0x%04" PRIX32,
                       this->voc_baselines_storage_.state0, this->voc_baselines_storage_.state1);
            } else {
              ESP_LOGW(TAG, "Could not store VOC baselines");
            }
          }
        }
      });
    }
  }

  uint16_t read_cmd = SEN6X_CMD_READ_MEASUREMENT;
  uint8_t read_words = 9;

  switch (this->sen6x_type_) {
    case SEN62:
      read_cmd = SEN62_CMD_READ_MEASUREMENT;
      read_words = 6;
      break;

    case SEN63C:
      read_cmd = SEN63C_CMD_READ_MEASUREMENT;
      read_words = 7;
      break;
    case SEN65:
      read_cmd = SEN65_CMD_READ_MEASUREMENT;
      read_words = 8;
      break;
    case SEN66:
      read_cmd = SEN66_CMD_READ_MEASUREMENT;
      read_words = 9;
      break;
    case SEN68:
      read_cmd = SEN68_CMD_READ_MEASUREMENT;
      read_words = 9;
      break;
    case SEN69C:
      read_cmd = SEN69C_CMD_READ_MEASUREMENT;
      read_words = 10;
      break;
    default:
      read_cmd = SEN66_CMD_READ_MEASUREMENT;
      read_words = 9;
      break;
  }

  const uint8_t poll_retries = 24;
  auto poll_ready = std::make_shared<std::function<void(uint8_t)>>();
  *poll_ready = [this, poll_ready, poll_retries, read_cmd, read_words](uint8_t retries_left) {
    const uint8_t attempt = static_cast<uint8_t>(poll_retries - retries_left + 1);
    ESP_LOGV(TAG, "Data ready polling attempt %u", attempt);
    uint16_t raw_read_status;
    if (!this->get_register(SEN6X_CMD_GET_DATA_READY_STATUS, raw_read_status, 20)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "read data ready status error (%d)", this->last_error_);
      return;
    }

    if ((raw_read_status & 0x0001) == 0) {
      if (retries_left == 0) {
        this->status_set_warning();
        ESP_LOGD(TAG, "data not ready in time");
        return;
      }
      this->set_timeout(50, [this, poll_ready, retries_left]() { (*poll_ready)(retries_left - 1); });
      return;
    }

    if (!this->write_command(read_cmd)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "write error read measurement (%d)", this->last_error_);
      return;
    }

    this->set_timeout(20, [this, read_words]() {
      uint16_t measurements[10];

      if (!this->read_data(measurements, read_words)) {
        this->status_set_warning();
        ESP_LOGD(TAG, "read data error (%d)", this->last_error_);
        return;
      }
      int8_t voc_index = -1;
      int8_t nox_index = -1;
      int8_t hcho_index = -1;
      int8_t co2_index = -1;
      bool has_ambient_pressure = false;
      bool co2_uint16 = false;
      switch (this->sen6x_type_) {
        case SEN62:
          break;
        case SEN63C:
          co2_index = 6;
          has_ambient_pressure = true;
          break;
        case SEN65:
          voc_index = 6;
          nox_index = 7;
          break;
        case SEN66:
          voc_index = 6;
          nox_index = 7;
          co2_index = 8;
          has_ambient_pressure = true;
          co2_uint16 = true;
          break;
        case SEN68:
          voc_index = 6;
          nox_index = 7;
          hcho_index = 8;
          break;
        case SEN69C:
          voc_index = 6;
          nox_index = 7;
          hcho_index = 8;
          co2_index = 9;
          has_ambient_pressure = true;
          break;
        default:
          break;
      }

      float pm_1_0 = measurements[0] / 10.0f;
      if (measurements[0] == 0xFFFF)
        pm_1_0 = NAN;
      float pm_2_5 = measurements[1] / 10.0f;
      if (measurements[1] == 0xFFFF)
        pm_2_5 = NAN;
      float pm_4_0 = measurements[2] / 10.0f;
      if (measurements[2] == 0xFFFF)
        pm_4_0 = NAN;
      float pm_10_0 = measurements[3] / 10.0f;
      if (measurements[3] == 0xFFFF)
        pm_10_0 = NAN;
      float humidity = static_cast<int16_t>(measurements[4]) / 100.0f;
      if (measurements[4] == 0x7FFF)
        humidity = NAN;
      float temperature = static_cast<int16_t>(measurements[5]) / 200.0f;
      if (measurements[5] == 0x7FFF)
        temperature = NAN;

      float voc = NAN;
      float nox = NAN;
      float hcho = NAN;
      float co2 = NAN;

      if (voc_index >= 0) {
        voc = static_cast<int16_t>(measurements[voc_index]) / 10.0f;
        if (measurements[voc_index] == 0x7FFF)
          voc = NAN;
      }
      if (nox_index >= 0) {
        nox = static_cast<int16_t>(measurements[nox_index]) / 10.0f;
        if (measurements[nox_index] == 0x7FFF)
          nox = NAN;
      }

      if (hcho_index >= 0) {
        const uint16_t hcho_raw = measurements[hcho_index];
        hcho = hcho_raw / 10.0f;
        if (hcho_raw == 0xFFFF)
          hcho = NAN;
      }

      if (co2_index >= 0) {
        if (co2_uint16) {
          const uint16_t co2_raw = measurements[co2_index];
          co2 = static_cast<float>(co2_raw);
          if (co2_raw == 0xFFFF)
            co2 = NAN;
        } else {
          const int16_t co2_raw = static_cast<int16_t>(measurements[co2_index]);
          co2 = static_cast<float>(co2_raw);
          if (co2_raw == 0x7FFF)
            co2 = NAN;
        }
      }

      this->last_pm_1_0_ = pm_1_0;
      this->last_pm_2_5_ = pm_2_5;
      this->last_pm_4_0_ = pm_4_0;
      this->last_pm_10_0_ = pm_10_0;
      this->last_temperature_ = temperature;
      this->last_humidity_ = humidity;
      this->last_voc_ = voc;
      this->last_nox_ = nox;
      this->last_hcho_ = hcho;
      this->last_co2_ = co2;
      this->has_last_values_ = true;

      const uint32_t check_time = App.get_loop_component_start_time();
      if (check_time < this->startup_stable_after_) {
        ESP_LOGV(TAG, "Startup stabilization in progress, skipping publish");
        this->status_clear_warning();
        return;
      }

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
      if (this->voc_sensor_ != nullptr && voc_index >= 0)
        this->voc_sensor_->publish_state(voc);
      if (this->nox_sensor_ != nullptr && nox_index >= 0)
        this->nox_sensor_->publish_state(nox);
      if (this->hcho_sensor_ != nullptr && hcho_index >= 0)
        this->hcho_sensor_->publish_state(hcho);
      if (this->co2_sensor_ != nullptr && co2_index >= 0)
        this->co2_sensor_->publish_state(co2);

      this->status_clear_warning();
    });
  };

  (*poll_ready)(poll_retries);
}

bool SEN6XComponent::write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning) {
  uint16_t params[6];
  params[0] = tuning.index_offset;
  params[1] = tuning.learning_time_offset_hours;
  params[2] = tuning.learning_time_gain_hours;
  params[3] = tuning.gating_max_duration_minutes;
  params[4] = tuning.std_initial;
  params[5] = tuning.gain_factor;
  auto result = this->write_command(i2c_command, params, 6);
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
  if (!this->write_command(SEN6X_CMD_TEMPERATURE_COMPENSATION, params, 4)) {
    ESP_LOGE(TAG, "set temperature_compensation failed. Err=%d", this->last_error_);
    return false;
  }
  return true;
}

bool SEN6XComponent::write_temperature_acceleration_(const TemperatureAcceleration &acceleration) {
  uint16_t params[4];
  params[0] = acceleration.k;
  params[1] = acceleration.p;
  params[2] = acceleration.t1;
  params[3] = acceleration.t2;
  if (!this->write_command(SEN6X_CMD_RHT_ACCELERATION_MODE, params, 4)) {
    ESP_LOGE(TAG, "set temperature_acceleration failed. Err=%d", this->last_error_);
    return false;
  }
  return true;
}

bool SEN6XComponent::start_fan_cleaning() {
  const bool was_running = this->measurement_started_;
  if (was_running) {
    this->stop_measurement();
  }
  if (!this->write_command(SEN6X_CMD_START_CLEANING_FAN)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start fan (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Fan auto clean started");
  }
  if (was_running) {
    this->auto_clean_restart_pending_ = true;
    this->set_timeout(15000, [this]() {
      if (this->auto_clean_restart_pending_) {
        this->start_measurement();
      }
    });
  }
  return true;
}

bool SEN6XComponent::perform_forced_co2_recalibration(uint16_t reference_ppm) {
  if (this->measurement_started_) {
    this->stop_measurement();
  }
  uint16_t params[1];
  params[0] = reference_ppm;
  if (!this->write_command(SEN6X_CMD_CO2_FORCE_RECALIBRATION, params, 1)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error forced CO2 recalibration (%d)", this->last_error_);
    return false;
  }
  this->set_timeout(500, [this]() {
    uint16_t correction = 0;
    if (!this->read_data(correction)) {
      this->status_set_warning();
      ESP_LOGE(TAG, "read error forced CO2 recalibration (%d)", this->last_error_);
      return;
    }
    if (correction == 0xFFFF) {
      ESP_LOGW(TAG, "Forced CO2 recalibration failed");
      return;
    }
    int16_t frc = static_cast<int16_t>(correction - 0x8000);
    ESP_LOGD(TAG, "Forced CO2 recalibration correction: %d ppm", frc);
  });
  return true;
}

bool SEN6XComponent::co2_sensor_factory_reset() {
  if (this->measurement_started_) {
    this->stop_measurement();
  }
  if (!this->write_command(SEN6X_CMD_CO2_SENSOR_FACTORY_RESET)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error CO2 sensor factory reset (%d)", this->last_error_);
    return false;
  }
  this->set_timeout(1400, [this]() { ESP_LOGD(TAG, "CO2 sensor factory reset complete"); });
  return true;
}

bool SEN6XComponent::start_measurement() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_stop_ms_ != 0 && (now - this->last_stop_ms_) < 1400) {
    const uint32_t wait_ms = 1400 - (now - this->last_stop_ms_);
    this->set_timeout(wait_ms, [this]() { this->start_measurement(); });
    return true;
  }
  if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start measurement (%d)", this->last_error_);
    return false;
  }
  this->measurement_started_ = true;
  this->startup_stable_after_ = now + this->startup_delay_ms_;
  this->last_cleaning_ms_ = now;
  if (this->measurement_running_binary_sensor_ != nullptr) {
    this->measurement_running_binary_sensor_->publish_state(true);
  }
  ESP_LOGD(TAG, "Measurement started");
  return true;
}

bool SEN6XComponent::stop_measurement() {
  if (!this->write_command(SEN6X_CMD_STOP_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error stop measurement (%d)", this->last_error_);
    return false;
  }
  this->measurement_started_ = false;
  this->last_stop_ms_ = App.get_loop_component_start_time();
  this->auto_clean_restart_pending_ = false;
  if (this->measurement_running_binary_sensor_ != nullptr) {
    this->measurement_running_binary_sensor_->publish_state(false);
  }
  ESP_LOGD(TAG, "Measurement stopped");
  return true;
}

bool SEN6XComponent::activate_sht_heater() {
  if (this->measurement_started_) {
    this->stop_measurement();
  }
  if (!this->write_command(SEN6X_CMD_SHT_HEATER_ACTIVATE)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error SHT heater activate (%d)", this->last_error_);
    return false;
  }
  this->set_timeout(20, [this]() { ESP_LOGD(TAG, "SHT heater activated"); });
  return true;
}

bool SEN6XComponent::get_sht_heater_measurements() {
  uint16_t min_fw = 0;
  switch (this->sen6x_type_) {
    case SEN62:
      min_fw = 6;
      break;
    case SEN63C:
      min_fw = 5;
      break;
    case SEN65:
      min_fw = 5;
      break;
    case SEN66:
      min_fw = 4;
      break;
    case SEN68:
      min_fw = 7;
      break;
    case SEN69C:
      min_fw = 9;
      break;
    default:
      break;
  }
  if (min_fw != 0 && this->firmware_version_ < min_fw) {
    ESP_LOGW(TAG, "SHT heater measurements not supported on firmware %u (min %u)", this->firmware_version_, min_fw);
    return false;
  }
  if (this->measurement_started_) {
    this->stop_measurement();
  }

  if (!this->write_command(SEN6X_CMD_SHT_HEATER_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error SHT heater measurements (%d)", this->last_error_);
    return false;
  }
  this->set_timeout(20, [this]() {
    uint16_t values[2];
    if (!this->read_data(values, 2)) {
      this->status_set_warning();
      ESP_LOGE(TAG, "read error SHT heater measurements (%d)", this->last_error_);
      return;
    }

    float humidity = static_cast<int16_t>(values[0]) / 100.0f;
    float temperature = static_cast<int16_t>(values[1]) / 200.0f;
    if (values[0] == 0x7FFF) {
      ESP_LOGD(TAG, "SHT heater humidity not ready");
    }
    if (values[1] == 0x7FFF) {
      ESP_LOGD(TAG, "SHT heater temperature not ready");
    }
    ESP_LOGD(TAG, "SHT heater measurements: RH=%.2f%% T=%.2fC", humidity, temperature);
  });
  return true;
}

}  // namespace sen6x
}  // namespace esphome

#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cmath>
#include <vector>

namespace esphome::sen6x {

static const char *const TAG = "sen6x";

static constexpr uint8_t POLL_RETRIES = 24;     // 24 attempts
static constexpr uint32_t I2C_READ_DELAY = 20;  // 20 ms to wait for I2C read to complete
static constexpr uint32_t POLL_INTERVAL = 50;   // 50 ms between poll attempts
// Single numeric timeout ID — the chain is sequential so only one is active at a time.
static constexpr uint32_t TIMEOUT_POLL = 1;
static constexpr uint32_t TIMEOUT_SETUP_STEP = 2;

static constexpr uint16_t SEN6X_CMD_GET_DATA_READY_STATUS = 0x0202;
static constexpr uint16_t SEN6X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static constexpr uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static constexpr uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;
static constexpr uint16_t SEN6X_CMD_NOX_ALGORITHM_TUNING = 0x60E1;

static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT = 0x0300;  // SEN66 only!
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN62 = 0x04A3;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN63C = 0x0471;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN65 = 0x0446;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN68 = 0x0467;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN69C = 0x04B5;
static constexpr uint16_t SEN6X_CMD_RHT_ACCELERATION_MODE = 0x6100;  // Set Temperature Acceleration Parameters

static constexpr uint16_t SEN6X_CMD_START_MEASUREMENTS = 0x0021;
static constexpr uint16_t SEN6X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static constexpr uint16_t SEN6X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;
static constexpr uint16_t SEN6X_CMD_CO2_SENSOR_AUTOMATIC_SELF_CAL = 0x6711;
static constexpr uint16_t SEN6X_CMD_AMBIENT_PRESSURE = 0x6720;
static constexpr uint16_t SEN6X_CMD_SENSOR_ALTITUDE = 0x6736;
static constexpr uint16_t SEN6X_CMD_RESET = 0xD304;

static constexpr int8_t SEN6X_INDEX_SCALE_FACTOR = 10;                         // used for VOC and NOx index values
static constexpr int8_t SEN6X_MIN_INDEX_VALUE = 1 * SEN6X_INDEX_SCALE_FACTOR;  // must be adjusted by the scale factor
static constexpr int16_t SEN6X_MAX_INDEX_VALUE =
    500 * SEN6X_INDEX_SCALE_FACTOR;  // must be adjusted by the scale factor

static inline void set_read_command_and_words(SEN6XComponent::Sen6xType type, uint16_t &read_cmd, uint8_t &read_words) {
  read_cmd = SEN6X_CMD_READ_MEASUREMENT;
  read_words = 9;
  switch (type) {
    case SEN6XComponent::SEN62:
      read_cmd = SEN6X_CMD_READ_MEASUREMENT_SEN62;
      read_words = 6;
      break;
    case SEN6XComponent::SEN63C:
      read_cmd = SEN6X_CMD_READ_MEASUREMENT_SEN63C;
      read_words = 7;
      break;
    case SEN6XComponent::SEN65:
      read_cmd = SEN6X_CMD_READ_MEASUREMENT_SEN65;
      read_words = 8;
      break;
    case SEN6XComponent::SEN66:
      read_cmd = SEN6X_CMD_READ_MEASUREMENT;
      read_words = 9;
      break;
    case SEN6XComponent::SEN68:
      read_cmd = SEN6X_CMD_READ_MEASUREMENT_SEN68;
      read_words = 9;
      break;
    case SEN6XComponent::SEN69C:
      read_cmd = SEN6X_CMD_READ_MEASUREMENT_SEN69C;
      read_words = 10;
      break;
    default:
      break;
  }
}

void SEN6XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen6x...");

  // the sensor needs 100 ms to enter the idle state
  this->set_timeout(100, [this]() {
    // Reset the sensor to ensure a clean state regardless of prior commands or power issues
    if (!this->write_command(SEN6X_CMD_RESET)) {
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      return;
    }

    // After reset the sensor needs 100 ms to become ready
    this->set_timeout(100, [this]() {
      // Step 1: Read serial number (~25ms with I2C delay)
      uint16_t raw_serial_number[16];
      if (!this->get_register(SEN6X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 16, 20)) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
        return;
      }
      this->serial_number_ = SEN6XComponent::sensirion_convert_to_string_in_place(raw_serial_number, 16);
      ESP_LOGI(TAG, "Serial number: %s", this->serial_number_.c_str());

      // Step 2: Read product name - use non-zero delay to avoid chaining blocking I2C reads in one loop tick
      this->set_timeout(20, [this]() {
        uint16_t raw_product_name[16];
        if (!this->get_register(SEN6X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }

        this->product_name_ = SEN6XComponent::sensirion_convert_to_string_in_place(raw_product_name, 16);

        Sen6xType inferred_type = this->infer_type_from_product_name_(this->product_name_);
        if (this->sen6x_type_ == UNKNOWN) {
          this->sen6x_type_ = inferred_type;
          if (inferred_type == UNKNOWN) {
            ESP_LOGE(TAG, "Unknown product '%s'", this->product_name_.c_str());
            this->mark_failed();
            return;
          }
          ESP_LOGD(TAG, "Type inferred from product: %s", this->product_name_.c_str());
        } else if (this->sen6x_type_ != inferred_type && inferred_type != UNKNOWN) {
          ESP_LOGW(TAG, "Configured type (used) mismatches product '%s'", this->product_name_.c_str());
        }
        ESP_LOGI(TAG, "Product: %s", this->product_name_.c_str());

        // Validate configured sensors against detected type and disable unsupported ones
        const bool has_voc_nox = (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 ||
                                  this->sen6x_type_ == SEN68 || this->sen6x_type_ == SEN69C);
        const bool has_co2 = (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);
        const bool has_hcho = (this->sen6x_type_ == SEN68 || this->sen6x_type_ == SEN69C);
        if (this->voc_sensor_ && !has_voc_nox) {
          ESP_LOGE(TAG, "VOC requires SEN65, SEN66, SEN68, or SEN69C");
          this->voc_sensor_ = nullptr;
        }
        if (this->nox_sensor_ && !has_voc_nox) {
          ESP_LOGE(TAG, "NOx requires SEN65, SEN66, SEN68, or SEN69C");
          this->nox_sensor_ = nullptr;
        }
        if (this->co2_sensor_ && !has_co2) {
          ESP_LOGE(TAG, "CO2 requires SEN63C, SEN66, or SEN69C");
          this->co2_sensor_ = nullptr;
        }
        if (this->hcho_sensor_ && !has_hcho) {
          ESP_LOGE(TAG, "Formaldehyde requires SEN68 or SEN69C");
          this->hcho_sensor_ = nullptr;
        }

        // Step 3: Read firmware version - use non-zero delay to avoid chaining blocking I2C reads in one loop tick
        this->set_timeout(20, [this]() {
          uint16_t raw_firmware_version = 0;
          if (!this->get_register(SEN6X_CMD_GET_FIRMWARE_VERSION, raw_firmware_version, 20)) {
            ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
            this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
            return;
          }
          this->firmware_version_major_ = (raw_firmware_version >> 8) & 0xFF;
          this->firmware_version_minor_ = raw_firmware_version & 0xFF;
          ESP_LOGI(TAG, "Firmware: %u.%u", this->firmware_version_major_, this->firmware_version_minor_);

          this->schedule_post_setup_commands_();
        });
      });
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

  // Read back CO2-related settings to confirm what the device has applied
  const bool supports_co2 = this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C;
  if (supports_co2) {
    steps.emplace_back([this]() {
      uint16_t ambient_pressure = 0;
      if (this->get_register(SEN6X_CMD_AMBIENT_PRESSURE, ambient_pressure, 20))
        if (ambient_pressure != 0xFFFF)
          this->ambient_pressure_read_ = ambient_pressure;
    });
    steps.emplace_back([this]() {
      uint16_t sensor_altitude = 0;
      if (this->get_register(SEN6X_CMD_SENSOR_ALTITUDE, sensor_altitude, 20))
        if (sensor_altitude != 0xFFFF)
          this->sensor_altitude_read_ = sensor_altitude;
    });
    steps.emplace_back([this]() {
      uint16_t asc_raw = 0;
      if (this->get_register(SEN6X_CMD_CO2_SENSOR_AUTOMATIC_SELF_CAL, asc_raw, 20))
        this->co2_asc_read_ = (asc_raw & 0x00FF) != 0;
    });
  }

  if (steps.empty()) {
    this->finish_setup_();
    return;
  }

  this->setup_steps_ = std::move(steps);
  this->setup_step_index_ = 0;
  this->run_next_setup_step_();
}

void SEN6XComponent::run_next_setup_step_() {
  if (this->setup_step_index_ >= this->setup_steps_.size()) {
    this->setup_steps_.clear();
    this->finish_setup_();
    return;
  }
  this->setup_steps_[this->setup_step_index_]();
  this->setup_step_index_++;
  this->set_timeout(TIMEOUT_SETUP_STEP, I2C_READ_DELAY, [this]() { this->run_next_setup_step_(); });
}

void SEN6XComponent::finish_setup_() {
  if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  this->set_timeout(this->startup_delay_ms_, [this]() { this->startup_complete_ = true; });
  this->initialized_ = true;
  ESP_LOGD(TAG, "Initialized");
}

void SEN6XComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "sen6x:\n"
                "  Product: %s\n"
                "  Serial: %s\n"
                "  Firmware: %u.%u\n"
                "  Address: 0x%02X",
                this->product_name_.c_str(), this->serial_number_.c_str(), this->firmware_version_major_,
                this->firmware_version_minor_, this->address_);
  LOG_UPDATE_INTERVAL(this);
  if (this->ambient_pressure_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Dynamic ambient pressure compensation using '%s'",
                  this->ambient_pressure_source_->get_name().c_str());
  } else if (this->ambient_pressure_.has_value()) {
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
  if (this->ambient_pressure_source_ != nullptr) {
    float pressure = this->ambient_pressure_source_->state;
    if (!std::isnan(pressure)) {
      uint16_t pressure_hpa = static_cast<uint16_t>(lroundf(pressure));
      if (!this->ambient_pressure_.has_value() || this->ambient_pressure_.value() != pressure_hpa) {
        this->update_ambient_pressure_compensation_(pressure_hpa);
      }
    }
  }
  // Cancel any in-flight polling from a previous update() cycle.
  this->cancel_timeout(TIMEOUT_POLL);

  set_read_command_and_words(this->sen6x_type_, this->read_cmd_, this->read_words_);

  // Polling uses chained timeouts to guarantee each I2C operation completes
  // before the next begins. The flow is:
  //
  //   poll_data_ready_()
  //     -> write_command (data ready status)
  //     -> timeout I2C_READ_DELAY
  //       -> read_data (check ready flag)
  //       -> if not ready: timeout POLL_INTERVAL -> poll_data_ready_() (retry)
  //       -> if ready: read_measurements_()
  //                      -> write_command (read measurement)
  //                      -> timeout I2C_READ_DELAY
  //                        -> parse_and_publish_measurements_()
  //
  // All timeouts share a single ID (TIMEOUT_POLL) since only one is active
  // at a time. cancel_timeout in update() stops any in-flight chain.
  this->poll_retries_remaining_ = POLL_RETRIES;
  this->poll_data_ready_();
}

void SEN6XComponent::poll_data_ready_() {
  if (this->poll_retries_remaining_ == 0) {
    this->status_set_warning();
    ESP_LOGD(TAG, "Data not ready");
    return;
  }
  ESP_LOGV(TAG, "Data ready polling attempt %u",
           static_cast<unsigned>(POLL_RETRIES - this->poll_retries_remaining_ + 1));
  this->poll_retries_remaining_--;

  if (!this->write_command(SEN6X_CMD_GET_DATA_READY_STATUS)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "write data ready status error (%d)", this->last_error_);
    return;
  }

  this->set_timeout(TIMEOUT_POLL, I2C_READ_DELAY, [this]() {
    uint16_t raw_read_status;
    if (!this->read_data(&raw_read_status, 1)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "read data ready status error (%d)", this->last_error_);
      return;
    }

    if ((raw_read_status & 0x0001) == 0) {
      // Not ready yet; schedule next attempt after POLL_INTERVAL.
      this->set_timeout(TIMEOUT_POLL, POLL_INTERVAL, [this]() { this->poll_data_ready_(); });
      return;
    }

    this->read_measurements_();
  });
}

void SEN6XComponent::read_measurements_() {
  if (!this->write_command(this->read_cmd_)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "Read measurement failed (%d)", this->last_error_);
    return;
  }

  this->set_timeout(TIMEOUT_POLL, I2C_READ_DELAY, [this]() { this->parse_and_publish_measurements_(); });
}

void SEN6XComponent::parse_and_publish_measurements_() {
  uint16_t measurements[10];

  if (!this->read_data(measurements, this->read_words_)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "Read data failed (%d)", this->last_error_);
    return;
  }
  int8_t voc_index = -1;
  int8_t nox_index = -1;
  int8_t hcho_index = -1;
  int8_t co2_index = -1;
  bool co2_uint16 = false;
  switch (this->sen6x_type_) {
    case SEN62:
      break;
    case SEN63C:
      co2_index = 6;
      break;
    case SEN65:
      voc_index = 6;
      nox_index = 7;
      break;
    case SEN66:
      voc_index = 6;
      nox_index = 7;
      co2_index = 8;
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

  if (!this->startup_complete_) {
    ESP_LOGD(TAG, "Startup delay, ignoring values");
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
  if (this->voc_sensor_ != nullptr)
    this->voc_sensor_->publish_state(voc);
  if (this->nox_sensor_ != nullptr)
    this->nox_sensor_->publish_state(nox);
  if (this->hcho_sensor_ != nullptr)
    this->hcho_sensor_->publish_state(hcho);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(co2);

  this->status_clear_warning();
}

bool SEN6XComponent::update_ambient_pressure_compensation_(uint16_t pressure_hpa) {
  if (pressure_hpa < 700 || pressure_hpa > 1200) {
    ESP_LOGW(TAG, "Ambient pressure out of range: %u hPa", pressure_hpa);
    return false;
  }
  if (this->ambient_pressure_.has_value() && this->ambient_pressure_.value() == pressure_hpa) {
    return true;
  }
  uint16_t params[1];
  params[0] = pressure_hpa;
  if (!this->write_command(SEN6X_CMD_AMBIENT_PRESSURE, params, 1)) {
    ESP_LOGE(TAG, "set ambient pressure failed. Err=%d", this->last_error_);
    return false;
  }
  this->ambient_pressure_ = pressure_hpa;
  return true;
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
    ESP_LOGE(TAG, "Set tuning parameters failed (command=0x%04X, err=%d)", i2c_command, this->last_error_);
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

SEN6XComponent::Sen6xType SEN6XComponent::infer_type_from_product_name_(const std::string &product_name) {
  if (product_name == "SEN62")
    return SEN62;
  if (product_name == "SEN63C")
    return SEN63C;
  if (product_name == "SEN65")
    return SEN65;
  if (product_name == "SEN66")
    return SEN66;
  if (product_name == "SEN68")
    return SEN68;
  if (product_name == "SEN69C")
    return SEN69C;
  return UNKNOWN;
}

}  // namespace esphome::sen6x

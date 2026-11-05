#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cinttypes>
#include <cmath>
#include <iterator>

namespace esphome::sen6x {

ESPHOME_LOG_TAG(TAG, "sen6x");

// Indexed by Sen6xType
PROGMEM_STRING_TABLE(TypeStrings, "SEN62", "SEN63C", "SEN65", "SEN66", "SEN68", "SEN69C", "UNKNOWN");

static SEN6XComponent::Sen6xType infer_type_from_product_name(const char *product_name) {
  const int16_t type = progmem_string_table_find<TypeStrings>(product_name, strnlen(product_name, 32));
  return type < 0 ? SEN6XComponent::UNKNOWN : static_cast<SEN6XComponent::Sen6xType>(type);
}

#ifdef SEN6X_STATUS_SENSOR_COUNT
// Bit of the status register behind each kind; the CO2 error bit is 9 on SEN66 and 12 on SEN63C/SEN69C
static uint8_t status_bit(Sen6xStatus status, SEN6XComponent::Sen6xType type) {
  switch (status) {
    case Sen6xStatus::SEN6X_STATUS_FAN_ERROR:
      return 4;
    case Sen6xStatus::SEN6X_STATUS_FAN_SPEED_WARNING:
      return 21;
    case Sen6xStatus::SEN6X_STATUS_RHT_ERROR:
      return 6;
    case Sen6xStatus::SEN6X_STATUS_GAS_ERROR:
      return 7;
    case Sen6xStatus::SEN6X_STATUS_CO2_ERROR:
      return type == SEN6XComponent::SEN66 ? 9 : 12;
    case Sen6xStatus::SEN6X_STATUS_HCHO_ERROR:
      return 10;
    case Sen6xStatus::SEN6X_STATUS_PM_ERROR:
      return 11;
  }
  return 0;
}
#endif

static constexpr uint8_t POLL_RETRIES = 24;     // 24 attempts
static constexpr uint32_t I2C_READ_DELAY = 20;  // 20 ms to wait for I2C read to complete
static constexpr uint32_t CMD_EXEC_DELAY = 20;  // execution time of set commands (datasheet section 4.8)
static constexpr uint32_t POLL_INTERVAL = 50;   // 50 ms between poll attempts
// Numeric timeout IDs. Each chain is sequential, so only one timeout per ID is active at a time.
static constexpr uint32_t TIMEOUT_POLL = 1;
static constexpr uint32_t TIMEOUT_SETUP_STEP = 2;
static constexpr uint32_t TIMEOUT_STARTUP = 3;
// Post-command wait times from the datasheet: start execution (4.8.1), stop execution (4.8.2),
// fan cleaning (4.8.22), SHT heater (4.8.23), CO2 conditioning after start on SEN63C/SEN69C (4.8.1)
static constexpr uint32_t START_MEASUREMENT_DELAY = 50;
static constexpr uint32_t STOP_MEASUREMENT_DELAY = 1400;
static constexpr uint32_t FAN_CLEANING_DELAY = 10000;
static constexpr uint32_t SHT_HEATER_DELAY = 20000;
static constexpr uint32_t CO2_CONDITIONING_DELAY = 24000;
// Accepted range of a runtime pressure source, the same as the schema's ambient_pressure_compensation
static constexpr float AMBIENT_PRESSURE_MIN_HPA = 700.0f;
static constexpr float AMBIENT_PRESSURE_MAX_HPA = 1200.0f;
static constexpr uint16_t SEN6X_CMD_GET_DATA_READY_STATUS = 0x0202;
static constexpr uint16_t SEN6X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static constexpr uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static constexpr uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;
static constexpr uint16_t SEN6X_CMD_READ_AND_CLEAR_DEVICE_STATUS = 0xD210;

static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT = 0x0300;  // SEN66 only!
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN62 = 0x04A3;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN63C = 0x0471;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN65 = 0x0446;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN68 = 0x0467;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN69C = 0x04B5;

#ifdef USE_SEN6X_NUMBER_CONCENTRATION
static constexpr uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;
#endif
static constexpr uint16_t SEN6X_CMD_START_MEASUREMENTS = 0x0021;
static constexpr uint16_t SEN6X_CMD_STOP_MEASUREMENTS = 0x0104;
static constexpr uint16_t SEN6X_CMD_START_FAN_CLEANING = 0x5607;
static constexpr uint16_t SEN6X_CMD_ACTIVATE_SHT_HEATER = 0x6765;
static constexpr uint16_t SEN6X_CMD_RESET = 0xD304;
static constexpr uint16_t SEN6X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;
static constexpr uint16_t SEN6X_CMD_NOX_ALGORITHM_TUNING = 0x60E1;
static constexpr uint16_t SEN6X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static constexpr uint16_t SEN6X_CMD_TEMPERATURE_ACCELERATION = 0x6100;
static constexpr uint16_t SEN6X_CMD_CO2_AUTOMATIC_SELF_CAL = 0x6711;
static constexpr uint16_t SEN6X_CMD_AMBIENT_PRESSURE = 0x6720;
static constexpr uint16_t SEN6X_CMD_SENSOR_ALTITUDE = 0x6736;

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
      // The last byte of serial_number_ stays 0, so a full 32-byte serial is still terminated
      memcpy(this->serial_number_, SEN6XComponent::sensirion_convert_to_string_in_place(raw_serial_number, 16),
             sizeof(this->serial_number_) - 1);
      ESP_LOGI(TAG, "Serial number: %s", this->serial_number_);

      // Step 2: Read product name in next loop iteration
      this->set_timeout(0, [this]() {
        // One spare zero word so a full 32-byte name is still terminated
        uint16_t raw_product_name[17]{};
        if (!this->get_register(SEN6X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
          ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
          this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
          return;
        }

        const char *product_name = SEN6XComponent::sensirion_convert_to_string_in_place(raw_product_name, 16);

        Sen6xType inferred_type = infer_type_from_product_name(product_name);
        if (this->sen6x_type_ == UNKNOWN) {
          this->sen6x_type_ = inferred_type;
          if (inferred_type == UNKNOWN) {
            ESP_LOGE(TAG, "Unknown product '%s'", product_name);
            this->mark_failed();
            return;
          }
          ESP_LOGD(TAG, "Type inferred from product: %s", product_name);
        } else if (this->sen6x_type_ != inferred_type && inferred_type != UNKNOWN) {
          ESP_LOGW(TAG, "Configured type (used) mismatches product '%s'", product_name);
        }
        ESP_LOGI(TAG, "Product: %s", product_name);

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
#ifdef SEN6X_STATUS_SENSOR_COUNT
        // A status sensor for a feature this variant lacks is dropped like the measurement sensors above, so
        // an empty table skips the status read; the kept ones learn their register bit here
        size_t kept = 0;
        for (auto entry : this->status_sensors_) {
          if ((entry.status == Sen6xStatus::SEN6X_STATUS_GAS_ERROR && !has_voc_nox) ||
              (entry.status == Sen6xStatus::SEN6X_STATUS_CO2_ERROR && !has_co2) ||
              (entry.status == Sen6xStatus::SEN6X_STATUS_HCHO_ERROR && !has_hcho)) {
            ESP_LOGE(TAG, "'%s' needs a variant with that feature", LOG_STR_ARG(entry.sensor->get_log_name()));
            continue;
          }
          entry.bit = status_bit(entry.status, this->sen6x_type_);
          this->status_sensors_[kept++] = entry;
        }
        this->status_sensors_.resize(kept);
#endif

        // Step 3: Read firmware version and start measurements in next loop iteration
        this->set_timeout(0, [this]() {
          uint16_t raw_firmware_version = 0;
          if (!this->get_register(SEN6X_CMD_GET_FIRMWARE_VERSION, raw_firmware_version, 20)) {
            ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
            this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
            return;
          }
          this->firmware_version_major_ = (raw_firmware_version >> 8) & 0xFF;
          this->firmware_version_minor_ = raw_firmware_version & 0xFF;
          ESP_LOGI(TAG, "Firmware: %u.%u", this->firmware_version_major_, this->firmware_version_minor_);

          // Step 4: write configuration commands one at a time, then start measurements.
          // Delay the first step so it doesn't run in the same loop tick as the read above.
          this->set_timeout(TIMEOUT_SETUP_STEP, CMD_EXEC_DELAY, [this]() { this->run_next_setup_step_(); });
        });
      });
    });
  });
}

// One configuration write per invocation, spaced by CMD_EXEC_DELAY, then the start command.
// These writes are optional, so a failure only warns and the chain carries on.
void SEN6XComponent::run_next_setup_step_() {
  while (this->setup_step_index_ < SETUP_STEP_COUNT) {
    if (this->write_setup_step_(static_cast<SetupStep>(this->setup_step_index_++))) {
      this->set_timeout(TIMEOUT_SETUP_STEP, CMD_EXEC_DELAY, [this]() { this->run_next_setup_step_(); });
      return;
    }
  }
  this->finish_setup_();
}

// Returns false when the step has nothing to write: the setting is not configured, or setup()
// disabled its sensor for this variant
bool SEN6XComponent::write_setup_step_(SetupStep step) {
  switch (step) {
    case SETUP_STEP_VOC_TUNING:
      if (this->voc_sensor_ == nullptr || !this->voc_tuning_params_.has_value())
        return false;
      this->write_tuning_parameters_(SEN6X_CMD_VOC_ALGORITHM_TUNING, this->voc_tuning_params_.value());
      return true;
    case SETUP_STEP_NOX_TUNING:
      if (this->nox_sensor_ == nullptr || !this->nox_tuning_params_.has_value())
        return false;
      this->write_tuning_parameters_(SEN6X_CMD_NOX_ALGORITHM_TUNING, this->nox_tuning_params_.value());
      return true;
    case SETUP_STEP_TEMPERATURE_COMPENSATION:
      if (!this->temperature_compensation_.has_value())
        return false;
      this->write_temperature_compensation_(this->temperature_compensation_.value());
      return true;
    case SETUP_STEP_TEMPERATURE_ACCELERATION:
      if (!this->temperature_acceleration_.has_value())
        return false;
      this->write_temperature_acceleration_(this->temperature_acceleration_.value());
      return true;
    case SETUP_STEP_CO2_ASC:
      if (this->co2_sensor_ == nullptr || !this->co2_asc_.has_value())
        return false;
      this->write_setup_register_(SEN6X_CMD_CO2_AUTOMATIC_SELF_CAL, this->co2_asc_.value() ? 1 : 0);
      return true;
    case SETUP_STEP_ALTITUDE:
      if (this->co2_sensor_ == nullptr || this->altitude_compensation_ == ALTITUDE_UNSET)
        return false;
      this->write_setup_register_(SEN6X_CMD_SENSOR_ALTITUDE, this->altitude_compensation_);
      return true;
    case SETUP_STEP_AMBIENT_PRESSURE:
      if (this->co2_sensor_ == nullptr || this->ambient_pressure_ == PRESSURE_UNSET)
        return false;
      this->write_ambient_pressure_(this->ambient_pressure_);
      return true;
    default:
      return false;
  }
}

bool SEN6XComponent::write_temperature_compensation_(const TemperatureCompensation &compensation) {
  // Word 4 selects compensation slot 0; other slots are not exposed
  uint16_t params[4] = {static_cast<uint16_t>(compensation.offset),
                        static_cast<uint16_t>(compensation.normalized_offset_slope), compensation.time_constant, 0};
  return this->write_config_words_(SEN6X_CMD_TEMPERATURE_COMPENSATION, params, 4);
}

bool SEN6XComponent::write_temperature_acceleration_(const TemperatureAcceleration &acceleration) {
  uint16_t params[4] = {acceleration.k, acceleration.p, acceleration.t1, acceleration.t2};
  return this->write_config_words_(SEN6X_CMD_TEMPERATURE_ACCELERATION, params, 4);
}

bool SEN6XComponent::write_setup_register_(uint16_t i2c_command, uint16_t value) {
  return this->write_config_words_(i2c_command, &value, 1);
}

void SEN6XComponent::finish_setup_() {
  if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Write 0x%04X failed, error %d", SEN6X_CMD_START_MEASUREMENTS, this->last_error_);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  this->on_measurement_started_();
  this->initialized_ = true;
  ESP_LOGD(TAG, "Initialized");
}

// Writes one optional configuration command and reserves its execution time. A failure warns and
// returns false, but does not stop setup: the sensor still measures with that setting left at its default.
bool SEN6XComponent::write_config_words_(uint16_t i2c_command, const uint16_t *data, uint8_t len) {
  if (!this->write_command(i2c_command, data, len)) {
    ESP_LOGE(TAG, "Write 0x%04X failed, error %d", i2c_command, this->last_error_);
    this->status_set_warning();
    return false;
  }
  this->set_command_wait_(CMD_EXEC_DELAY);
  return true;
}

bool SEN6XComponent::write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning) {
  uint16_t params[6] = {tuning.index_offset,
                        tuning.learning_time_offset_hours,
                        tuning.learning_time_gain_hours,
                        tuning.gating_max_duration_minutes,
                        tuning.std_initial,
                        tuning.gain_factor};
  return this->write_config_words_(i2c_command, params, 6);
}

void SEN6XComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "sen6x:\n"
                "  Type: %s\n"
                "  Serial: %s\n"
                "  Firmware: %u.%u\n"
                "  Address: 0x%02X\n"
                "  Startup delay: %" PRIu32 " ms",
                LOG_STR_ARG(TypeStrings::get_log_str(this->sen6x_type_, TypeStrings::LAST_INDEX)), this->serial_number_,
                this->firmware_version_major_, this->firmware_version_minor_, this->address_, this->startup_delay_ms_);
  LOG_UPDATE_INTERVAL(this);
  if (this->temperature_compensation_.has_value()) {
    const auto &comp = this->temperature_compensation_.value();
    ESP_LOGCONFIG(TAG, "  Temperature compensation: offset=%.2f slope=%.4f time_constant=%us", comp.offset / 200.0f,
                  comp.normalized_offset_slope / 10000.0f, comp.time_constant);
  }
  if (this->temperature_acceleration_.has_value()) {
    const auto &accel = this->temperature_acceleration_.value();
    ESP_LOGCONFIG(TAG, "  Temperature acceleration: K=%.1f P=%.1f T1=%.1f T2=%.1f", accel.k / 10.0f, accel.p / 10.0f,
                  accel.t1 / 10.0f, accel.t2 / 10.0f);
  }
  // setup() clears co2_sensor_ once it detects a variant without CO2
  if (this->co2_sensor_ != nullptr) {
    if (this->co2_asc_.has_value()) {
      ESP_LOGCONFIG(TAG, "  CO2 automatic self-calibration: %s", ONOFF(this->co2_asc_.value()));
    }
    if (this->altitude_compensation_ != ALTITUDE_UNSET) {
      ESP_LOGCONFIG(TAG, "  Altitude compensation: %u m", this->altitude_compensation_);
    }
    if (this->ambient_pressure_ != PRESSURE_UNSET) {
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %u hPa", this->ambient_pressure_);
    }
    if (this->ambient_pressure_source_sensor_ != nullptr) {
      ESP_LOGCONFIG(TAG, "  Ambient pressure compensation source: %s",
                    LOG_STR_ARG(this->ambient_pressure_source_sensor_->get_log_name()));
    }
  }
  LOG_SENSOR("  ", "PM  1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM  2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM  4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);
#ifdef USE_SEN6X_NUMBER_CONCENTRATION
  LOG_SENSOR("  ", "PMC  0.5", this->pmc_0_5_sensor_);
  LOG_SENSOR("  ", "PMC  1.0", this->pmc_1_0_sensor_);
  LOG_SENSOR("  ", "PMC  2.5", this->pmc_2_5_sensor_);
  LOG_SENSOR("  ", "PMC  4.0", this->pmc_4_0_sensor_);
  LOG_SENSOR("  ", "PMC 10.0", this->pmc_10_0_sensor_);
#endif
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);
  LOG_SENSOR("  ", "NOx", this->nox_sensor_);
  LOG_SENSOR("  ", "HCHO", this->hcho_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
#ifdef SEN6X_STATUS_SENSOR_COUNT
  for (const auto &entry : this->status_sensors_) {
    LOG_BINARY_SENSOR("  ", "Device status", entry.sensor);
  }
#endif
}

void SEN6XComponent::update() {
  if (!this->initialized_ || !this->measuring_) {
    return;
  }
  // Cancel any in-flight polling from a previous update() cycle before touching the bus.
  this->cancel_timeout(TIMEOUT_POLL);
  // A command is still executing (an action moments ago): run this cycle once its window has passed
  if (const uint32_t wait = this->command_wait_remaining_(); wait != 0) {
    this->set_timeout(TIMEOUT_POLL, wait, [this]() { this->update(); });
    return;
  }

  if (this->ambient_pressure_source_sensor_ != nullptr && this->co2_sensor_ != nullptr) {
    this->update_ambient_pressure_compensation_(this->ambient_pressure_source_sensor_->state);
  }

  set_read_command_and_words(this->sen6x_type_, this->read_cmd_, this->read_words_);

  // Polling uses chained timeouts to guarantee each I2C operation completes
  // before the next begins. The flow is:
  //
  //   read_device_status_()
  //     with status sensors: write_command (read and clear device status)
  //       -> timeout I2C_READ_DELAY -> parse_and_publish_device_status_()
  //     -> poll_data_ready_()
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
  //                          -> with pmc_* sensors: read_number_concentration_()
  //                               -> write_command (read number concentration)
  //                               -> timeout I2C_READ_DELAY
  //                                 -> parse_and_publish_number_concentration_()
  //
  // All timeouts share a single ID (TIMEOUT_POLL) since only one is active
  // at a time. cancel_timeout in update() stops any in-flight chain.
  this->poll_retries_remaining_ = POLL_RETRIES;
  this->read_device_status_();
}

void SEN6XComponent::poll_data_ready_() {
  // stop_measurement() may have run from a publish callback while the chain was in flight
  if (!this->measuring_)
    return;
  if (this->poll_retries_remaining_ == 0) {
    this->status_set_warning();
    ESP_LOGD(TAG, "Data not ready");
    return;
  }
  // A set command (the pressure write above all) gets its execution time before the chain writes again
  if (const uint32_t wait = this->command_wait_remaining_(); wait != 0) {
    this->set_timeout(TIMEOUT_POLL, wait, [this]() { this->poll_data_ready_(); });
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

  PUBLISH_STATE(this->pm_1_0_sensor_, pm_1_0);
  PUBLISH_STATE(this->pm_2_5_sensor_, pm_2_5);
  PUBLISH_STATE(this->pm_4_0_sensor_, pm_4_0);
  PUBLISH_STATE(this->pm_10_0_sensor_, pm_10_0);
  PUBLISH_STATE(this->temperature_sensor_, temperature);
  PUBLISH_STATE(this->humidity_sensor_, humidity);
  PUBLISH_STATE(this->voc_sensor_, voc);
  PUBLISH_STATE(this->nox_sensor_, nox);
  PUBLISH_STATE(this->hcho_sensor_, hcho);
  PUBLISH_STATE(this->co2_sensor_, co2);

#ifdef USE_SEN6X_NUMBER_CONCENTRATION
  // The define says some device in the build has pmc sensors; this one may not
  if (this->has_number_concentration_sensor_()) {
    this->read_number_concentration_();  // the chain ends there, with the warning cleared
    return;
  }
#endif
  this->status_clear_warning();
}

// Reads the status register first, so the error sensors update even when the measurement read fails, then runs
// the measurement poll chain. Read And Clear (0xD210) is used because error flags are sticky on the device;
// clearing each cycle makes the sensors report whether a problem occurred since the last update.
void SEN6XComponent::read_device_status_() {
#ifdef SEN6X_STATUS_SENSOR_COUNT
  if (!this->status_sensors_.empty()) {
    // The pressure write in update() may still be executing; the device rejects commands until it is done
    if (const uint32_t wait = this->command_wait_remaining_(); wait != 0) {
      this->set_timeout(TIMEOUT_POLL, wait, [this]() { this->read_device_status_(); });
      return;
    }
    if (this->write_command(SEN6X_CMD_READ_AND_CLEAR_DEVICE_STATUS)) {
      this->set_timeout(TIMEOUT_POLL, I2C_READ_DELAY, [this]() { this->parse_and_publish_device_status_(); });
      return;
    }
    ESP_LOGD(TAG, "Read device status failed (%d)", this->last_error_);
  }
#endif
  this->poll_data_ready_();
}

#ifdef SEN6X_STATUS_SENSOR_COUNT
void SEN6XComponent::parse_and_publish_device_status_() {
  uint16_t words[2];
  if (this->read_data(words, 2)) {
    const uint32_t status = (static_cast<uint32_t>(words[0]) << 16) | words[1];
    for (const auto &entry : this->status_sensors_)
      entry.sensor->publish_state(((status >> entry.bit) & 1) != 0);
  } else {
    ESP_LOGD(TAG, "Read device status failed (%d)", this->last_error_);
  }
  this->poll_data_ready_();
}
#endif

#ifdef USE_SEN6X_NUMBER_CONCENTRATION
bool SEN6XComponent::has_number_concentration_sensor_() const {
  return this->pmc_0_5_sensor_ != nullptr || this->pmc_1_0_sensor_ != nullptr || this->pmc_2_5_sensor_ != nullptr ||
         this->pmc_4_0_sensor_ != nullptr || this->pmc_10_0_sensor_ != nullptr;
}

void SEN6XComponent::read_number_concentration_() {
  if (!this->write_command(SEN6X_CMD_READ_NUMBER_CONCENTRATION)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "Read number concentration failed (%d)", this->last_error_);
    return;
  }

  this->set_timeout(TIMEOUT_POLL, I2C_READ_DELAY, [this]() { this->parse_and_publish_number_concentration_(); });
}

void SEN6XComponent::parse_and_publish_number_concentration_() {
  sensor::Sensor *sensors[] = {this->pmc_0_5_sensor_, this->pmc_1_0_sensor_, this->pmc_2_5_sensor_,
                               this->pmc_4_0_sensor_, this->pmc_10_0_sensor_};
  uint16_t measurements[std::size(sensors)];

  if (!this->status_comm_result(this->read_data(measurements, std::size(measurements)))) {
    ESP_LOGD(TAG, "Read data failed (%d)", this->last_error_);
    return;
  }

  for (size_t i = 0; i < std::size(sensors); i++) {
    PUBLISH_STATE(sensors[i], measurements[i] == 0xFFFF ? NAN : measurements[i] / 10.0f);
  }
}
#endif

void SEN6XComponent::write_ambient_pressure_(uint16_t pressure_hpa) {
  if (this->write_setup_register_(SEN6X_CMD_AMBIENT_PRESSURE, pressure_hpa))
    this->last_ambient_pressure_ = pressure_hpa;
}

void SEN6XComponent::update_ambient_pressure_compensation_(float pressure_hpa) {
  if (std::isnan(pressure_hpa))
    return;
  // Range-check before narrowing so out-of-unit sources (e.g. Pa) can't wrap into range
  if (pressure_hpa < AMBIENT_PRESSURE_MIN_HPA || pressure_hpa > AMBIENT_PRESSURE_MAX_HPA) {
    if (!this->pressure_range_warned_) {
      ESP_LOGW(TAG, "Ambient pressure out of range: %.0f hPa", pressure_hpa);
      this->pressure_range_warned_ = true;
    }
    return;
  }
  this->pressure_range_warned_ = false;
  const auto value = static_cast<uint16_t>(lroundf(pressure_hpa));
  if (value != this->last_ambient_pressure_)
    this->write_ambient_pressure_(value);
}

// Time left in the previous command's execution or settling window, 0 once it has passed
uint32_t SEN6XComponent::command_wait_remaining_() const {
  const uint32_t elapsed = millis() - this->command_started_at_;
  return elapsed < this->command_wait_ ? this->command_wait_ - elapsed : 0;
}

bool SEN6XComponent::command_blocked_() const { return this->command_wait_remaining_() != 0; }

void SEN6XComponent::set_command_wait_(uint32_t wait_ms) {
  this->command_started_at_ = millis();
  this->command_wait_ = wait_ms;
}

void SEN6XComponent::on_measurement_started_() {
  this->measuring_ = true;
  this->set_command_wait_(START_MEASUREMENT_DELAY);
  // SEN63C/SEN69C condition the CO2 sensor for 24 s after a start; block restarts until then
  if (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN69C) {
    this->co2_started_at_ = millis();
    this->co2_wait_ = CO2_CONDITIONING_DELAY;
  }
  // Values need the warm-up period again after every start
  this->startup_complete_ = false;
  this->set_timeout(TIMEOUT_STARTUP, this->startup_delay_ms_, [this]() { this->startup_complete_ = true; });
}

void SEN6XComponent::start_measurement() {
  if (!this->initialized_ || this->measuring_)
    return;
  // The CO2 conditioning window only blocks restarting, not other commands
  if (this->command_blocked_() || millis() - this->co2_started_at_ < this->co2_wait_) {
    ESP_LOGW(TAG, "Device busy");
    return;
  }
  if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Start measurement failed (%d)", this->last_error_);
    return;
  }
  this->on_measurement_started_();
}

void SEN6XComponent::stop_measurement() {
  if (!this->initialized_ || !this->measuring_)
    return;
  if (this->command_blocked_()) {
    ESP_LOGW(TAG, "Device busy");
    return;
  }
  // Stop any in-flight polling before the device goes idle
  this->cancel_timeout(TIMEOUT_POLL);
  if (!this->write_command(SEN6X_CMD_STOP_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Stop measurement failed (%d)", this->last_error_);
    return;
  }
  this->measuring_ = false;
  this->set_command_wait_(STOP_MEASUREMENT_DELAY);
}

void SEN6XComponent::start_fan_cleaning() {
  if (!this->initialized_)
    return;
  // The device only accepts this command in idle mode (datasheet section 4.8.22)
  if (this->measuring_) {
    ESP_LOGW(TAG, "Fan cleaning requires idle mode; stop the measurement first");
    return;
  }
  if (this->command_blocked_()) {
    ESP_LOGW(TAG, "Device busy");
    return;
  }
  if (!this->write_command(SEN6X_CMD_START_FAN_CLEANING)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Fan cleaning failed (%d)", this->last_error_);
    return;
  }
  this->set_command_wait_(FAN_CLEANING_DELAY);
}

void SEN6XComponent::activate_sht_heater() {
  if (!this->initialized_)
    return;
  // The device only accepts this command in idle mode (datasheet section 4.8.23)
  if (this->measuring_) {
    ESP_LOGW(TAG, "SHT heater requires idle mode; stop the measurement first");
    return;
  }
  if (this->command_blocked_()) {
    ESP_LOGW(TAG, "Device busy");
    return;
  }
  if (!this->write_command(SEN6X_CMD_ACTIVATE_SHT_HEATER)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "SHT heater failed (%d)", this->last_error_);
    return;
  }
  this->set_command_wait_(SHT_HEATER_DELAY);
}

}  // namespace esphome::sen6x

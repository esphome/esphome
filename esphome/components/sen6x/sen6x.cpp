#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::sen6x {

static const char *const TAG = "sen6x";

static constexpr uint8_t POLL_RETRIES = 24;
static constexpr uint32_t I2C_READ_DELAY = 20;
static constexpr uint32_t POLL_INTERVAL = 50;
static constexpr uint32_t TIMEOUT_POLL = 1;
static constexpr uint32_t INIT_TIME = 100;
static constexpr uint32_t STARTUP_TIME = 1100;
static constexpr uint32_t WARMUP_TIME = 60000;

// I2C Commands
static constexpr uint16_t SEN6X_CMD_GET_DATA_READY_STATUS = 0x0202;
static constexpr uint16_t SEN6X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static constexpr uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static constexpr uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;
static constexpr uint16_t SEN6X_CMD_READ_DEVICE_STATUS = 0xD206;
static constexpr uint16_t SEN6X_CMD_READ_AND_CLEAR_DEVICE_STATUS = 0xD210;
static constexpr uint16_t SEN6X_CMD_START_MEASUREMENTS = 0x0021;
static constexpr uint16_t SEN6X_CMD_STOP_MEASUREMENTS = 0x0104;
static constexpr uint16_t SEN6X_CMD_RESET = 0xD304;
static constexpr uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;

// Specific commands for continuous measurements by model
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN66 = 0x0300;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN62 = 0x04A3;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN63C = 0x0471;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN65 = 0x0446;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN68 = 0x0467;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN69C = 0x04B5;

// Tuning and parameters
static constexpr uint16_t SEN6X_CMD_SET_TEMPERATURE_OFFSET = 0x60B2;
static constexpr uint16_t SEN6X_CMD_SET_TEMPERATURE_ACCELERATION = 0x6100;
static constexpr uint16_t SEN6X_CMD_START_FAN_CLEANING = 0x5607;
static constexpr uint16_t SEN6X_CMD_ACTIVATE_SHT_HEATER = 0x6765;
static constexpr uint16_t SEN6X_CMD_GET_SHT_HEATER_MEASUREMENTS = 0x6790;
static constexpr uint16_t SEN6X_CMD_GET_VOC_ALGORITHM_TUNING_PARAMETERS = 0x60D0;
static constexpr uint16_t SEN6X_CMD_SET_VOC_ALGORITHM_TUNING_PARAMETERS = 0x60D0;
static constexpr uint16_t SEN6X_CMD_GET_NOX_ALGORITHM_TUNING_PARAMETERS = 0x60E1;
static constexpr uint16_t SEN6X_CMD_SET_NOX_ALGORITHM_TUNING_PARAMETERS = 0x60E1;

// CO2, Pressure e Altitude
static constexpr uint16_t SEN6X_CMD_SET_PERFORM_FORCED_CO2_RECALIBRATION = 0x6707;
static constexpr uint16_t SEN6X_CMD_SET_PERFORM_CO2_SENSOR_FACTORY_RESET = 0x6754;
static constexpr uint16_t SEN6X_CMD_GET_CO2_SENSOR_AUTOMATIC_SELF_CALIBRATION = 0x6711;
static constexpr uint16_t SEN6X_CMD_SET_CO2_SENSOR_AUTOMATIC_SELF_CALIBRATION = 0x6711;
static constexpr uint16_t SEN6X_CMD_GET_AMBIENT_PRESSURE = 0x6720;
static constexpr uint16_t SEN6X_CMD_SET_AMBIENT_PRESSURE = 0x6720;
static constexpr uint16_t SEN6X_CMD_GET_SENSOR_ALTITUDE = 0x6736;
static constexpr uint16_t SEN6X_CMD_SET_SENSOR_ALTITUDE = 0x6736;

static inline void set_read_command_and_words(SEN6XComponent::Sen6xType type, uint16_t &read_cmd, uint8_t &read_words) {
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
      read_cmd = SEN6X_CMD_READ_MEASUREMENT_SEN66;
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
    case SEN6XComponent::UNKNOWN:
      read_cmd = SEN6X_CMD_READ_MEASUREMENT_SEN66;
      read_words = 9;
      break;
  }
}

SEN6XComponent::Sen6xType SEN6XComponent::infer_type_from_product_name_(const std::string &product_name) {
  // Use a simple mapping to match product strings to types.
  // We check if the product name contains the model string to handle potential suffixes.
  if (product_name.find("SEN62") != std::string::npos)
    return SEN62;
  if (product_name.find("SEN63C") != std::string::npos)
    return SEN63C;
  if (product_name.find("SEN65") != std::string::npos)
    return SEN65;
  if (product_name.find("SEN66") != std::string::npos)
    return SEN66;
  if (product_name.find("SEN68") != std::string::npos)
    return SEN68;
  if (product_name.find("SEN69C") != std::string::npos)
    return SEN69C;
  // Log a warning if the product name was read but doesn't match known models
  if (!product_name.empty()) {
    ESP_LOGW(TAG, "Product name '%s' does not match any known SEN6x type", product_name.c_str());
  }
  return UNKNOWN;
}

void SEN6XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN6X...");

  // The sensor needs ~100ms after power-up to enter IDLE state and accept commands
  this->set_timeout(100, [this]() {
    // Reset the sensor to ensure a clean state regardless of prior commands or power issues
    if (!this->write_command(SEN6X_CMD_RESET)) {
      ESP_LOGE(TAG, "Failed to send reset command!");
      this->mark_failed();
      return;
    }

    // After a soft reset, the sensor needs another 100ms to become responsive
    this->set_timeout(100, [this]() {
      this->state_ = STARTING;

      // Step 1: Read serial number
      uint16_t raw_serial[16];
      if (!this->get_register(SEN6X_CMD_GET_SERIAL_NUMBER, raw_serial, 16, I2C_READ_DELAY)) {
        ESP_LOGE(TAG, "Failed to read serial number!");
        this->mark_failed();
        return;
      }
      this->serial_number_ = SEN6XComponent::sensirion_convert_to_string_in_place(raw_serial, 16);
      ESP_LOGI(TAG, "Serial number: %s", this->serial_number_.c_str());

      // Step 2: Read product name and validate sensor compatibility
      this->set_timeout(0, [this]() {
        uint16_t raw_name[16];
        if (!this->get_register(SEN6X_CMD_GET_PRODUCT_NAME, raw_name, 16, I2C_READ_DELAY)) {
          ESP_LOGE(TAG, "Failed to read product name!");
          this->mark_failed();
          return;
        }

        this->product_name_ = SEN6XComponent::sensirion_convert_to_string_in_place(raw_name, 16);
        Sen6xType inferred = this->infer_type_from_product_name_(this->product_name_);

        // Handle type detection and mismatch
        if (this->sen6x_type_ == UNKNOWN) {
          if (inferred == UNKNOWN) {
            ESP_LOGE(TAG, "Unsupported product detected: '%s'", this->product_name_.c_str());
            this->mark_failed();
            return;
          }
          this->sen6x_type_ = inferred;
          ESP_LOGD(TAG, "Type inferred from product name: %s", this->product_name_.c_str());
        } else if (inferred != UNKNOWN && this->sen6x_type_ != inferred) {
          ESP_LOGW(TAG, "Manual type configuration mismatches detected product '%s'", this->product_name_.c_str());
        }
        ESP_LOGI(TAG, "Product: %s", this->product_name_.c_str());

        // Define capabilities based on detected/configured type
        const bool has_voc_nox = (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 ||
                                  this->sen6x_type_ == SEN68 || this->sen6x_type_ == SEN69C);
        const bool has_co2 = (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);
        const bool has_hcho = (this->sen6x_type_ == SEN68 || this->sen6x_type_ == SEN69C);
        const bool has_nc = (this->sen6x_type_ != SEN69C);  // SEN69C is the only one lacking NC in the series

        // Disable unsupported sensors and log errors
        if (this->voc_sensor_ && !has_voc_nox) {
          ESP_LOGE(TAG, "VOC sensor not supported by %s - disabling", this->product_name_.c_str());
          this->voc_sensor_ = nullptr;
        }
        if (this->nox_sensor_ && !has_voc_nox) {
          ESP_LOGE(TAG, "NOx sensor not supported by %s - disabling", this->product_name_.c_str());
          this->nox_sensor_ = nullptr;
        }
        if (this->co2_sensor_ && !has_co2) {
          ESP_LOGE(TAG, "CO2 sensor not supported by %s - disabling", this->product_name_.c_str());
          this->co2_sensor_ = nullptr;
        }
        if (this->hcho_sensor_ && !has_hcho) {
          ESP_LOGE(TAG, "HCHO sensor not supported by %s - disabling", this->product_name_.c_str());
          this->hcho_sensor_ = nullptr;
        }
        if (!has_nc && (this->pm_nc_0_5_sensor_ || this->pm_nc_1_0_sensor_ || this->pm_nc_2_5_sensor_)) {
          ESP_LOGE(TAG, "NC sensors not supported by %s - disabling", this->product_name_.c_str());
          this->pm_nc_0_5_sensor_ = this->pm_nc_1_0_sensor_ = this->pm_nc_2_5_sensor_ = nullptr;
          this->pm_nc_4_0_sensor_ = this->pm_nc_10_0_sensor_ = nullptr;
        }

        // Step 3: Read firmware and start continuous measurement
        this->set_timeout(0, [this]() {
          uint16_t raw_fw = 0;
          if (!this->get_register(SEN6X_CMD_GET_FIRMWARE_VERSION, raw_fw, I2C_READ_DELAY)) {
            ESP_LOGE(TAG, "Failed to read firmware version!");
            this->mark_failed();
            return;
          }
          this->firmware_version_major_ = (raw_fw >> 8) & 0xFF;
          this->firmware_version_minor_ = raw_fw & 0xFF;
          ESP_LOGI(TAG, "Firmware: %u.%u", this->firmware_version_major_, this->firmware_version_minor_);

          if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
            ESP_LOGE(TAG, "Failed to start continuous measurements!");
            this->mark_failed();
            return;
          }

          this->state_ = WARMING_UP;
          ESP_LOGD(TAG, "Sensor warming up...");

          this->set_timeout(WARMUP_TIME, [this]() {
            this->state_ = MEASURING;
            ESP_LOGD(TAG, "Sensor initialized and measuring");
          });
        });
      });
    });
  });
}

void SEN6XComponent::dump_config() {
  // Header
  ESP_LOGCONFIG(TAG, "SEN6X:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Product: %s", this->product_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Serial: %s", this->serial_number_.c_str());
  ESP_LOGCONFIG(TAG, "  Firmware: %u.%u", this->firmware_version_major_, this->firmware_version_minor_);

  LOG_UPDATE_INTERVAL(this);

  // Mass Concentration
  LOG_SENSOR("  ", "PM 1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM 2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM 4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);

  // Number Concentration
  LOG_SENSOR("  ", "NC 0.5", this->pm_nc_0_5_sensor_);
  LOG_SENSOR("  ", "NC 1.0", this->pm_nc_1_0_sensor_);
  LOG_SENSOR("  ", "NC 2.5", this->pm_nc_2_5_sensor_);
  LOG_SENSOR("  ", "NC 4.0", this->pm_nc_4_0_sensor_);
  LOG_SENSOR("  ", "NC 10.0", this->pm_nc_10_0_sensor_);

  // Gases & Environment
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC Index", this->voc_sensor_);
  LOG_SENSOR("  ", "NOx Index", this->nox_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "HCHO", this->hcho_sensor_);
}

void SEN6XComponent::update() {
  // Skip update if the sensor is still initializing
  if (this->state_ == STARTING) {
    return;
  }

  // Ensure any in-flight polling chain from a previous update cycle is stopped
  this->cancel_timeout(TIMEOUT_POLL);

  // If the sensor is in IDLE, there's no data to fetch
  if (this->is_idle_()) {
    ESP_LOGV(TAG, "Skipping update: sensor is in IDLE state");
    return;
  }

  // Set the specific read command and word count based on the detected SEN6x model
  set_read_command_and_words(this->sen6x_type_, this->read_cmd_, this->read_words_);

  // Start the asynchronous polling chain using sequential timeouts.
  // Logic flow:
  //   poll_data_ready_() -> check flag -> if ready -> read_measurements_() -> parse/publish
  // All steps use I2C_READ_DELAY to ensure the sensor has time to process requests.

  this->poll_retries_remaining_ = POLL_RETRIES;

  if (this->is_measuring_()) {
    ESP_LOGV(TAG, "Starting data readout cycle...");
    this->poll_data_ready_();
  }
}

void SEN6XComponent::poll_data_ready_() {
  if (this->poll_retries_remaining_ == 0) {
    ESP_LOGW(TAG, "Data ready polling failed: maximum retries reached");
    return;
  }

  ESP_LOGV(TAG, "Polling data ready (attempt %u/%u)...",
           static_cast<unsigned>(POLL_RETRIES - this->poll_retries_remaining_ + 1),
           static_cast<unsigned>(POLL_RETRIES));

  this->poll_retries_remaining_--;

  // Step 1: Send the request for the Data Ready status
  if (!this->write_command(SEN6X_CMD_GET_DATA_READY_STATUS)) {
    ESP_LOGE(TAG, "Failed to write Data Ready status command! (Error: %d)", this->last_error_);
    return;
  }

  // Step 2: Wait for the sensor to process the request, then read the result
  this->set_timeout(TIMEOUT_POLL, I2C_READ_DELAY, [this]() {
    uint16_t status_reg;
    if (!this->read_data(&status_reg, 1)) {
      ESP_LOGE(TAG, "Failed to read Data Ready register! (Error: %d)", this->last_error_);
      return;
    }

    // Check the LSB (bit 0): 1 = Data Ready, 0 = Busy
    if ((status_reg & 0x0001) == 0) {
      // Data not ready yet; schedule the next polling attempt
      this->set_timeout(TIMEOUT_POLL, POLL_INTERVAL, [this]() { this->poll_data_ready_(); });
      return;
    }

    // Data is available, proceed to read measurements
    ESP_LOGV(TAG, "Data is ready, proceeding to readout");
    this->read_measurements_();
  });
}

void SEN6XComponent::read_measurements_() {
  // Step 1: Request the measurement data from the sensor.
  // The specific command (read_cmd_) was already determined in the update() cycle
  // based on the detected sensor type.
  if (!this->write_command(this->read_cmd_)) {
    ESP_LOGE(TAG, "Failed to send read measurement command! (Error: %d)", this->last_error_);
    return;
  }
  // Step 2: Wait for the sensor to prepare the data buffer (I2C_READ_DELAY).
  // Then proceed to parse and publish the values to the ESPHome frontend.
  this->set_timeout(TIMEOUT_POLL, I2C_READ_DELAY, [this]() { this->parse_and_publish_measurements_(); });
}

void SEN6XComponent::parse_and_publish_measurements_() {
  uint16_t m[10];

  // Check if we are in a valid state to process data
  if (!this->is_measuring_()) {
    ESP_LOGD(TAG, "Sensor is not in measuring state, ignoring data");
    return;
  }

  // Read the raw data words from the I2C bus
  if (!this->read_data(m, this->read_words_)) {
    ESP_LOGE(TAG, "Failed to read measurement data! (Error: %d)", this->last_error_);
    return;
  }

  // Helper lambda to handle Sensirion's NAN/Invalid values
  // auto check_nan = [](uint16_t value, uint16_t invalid_marker) -> float {
  // return (value == invalid_marker) ? NAN : NAN; // Placeholder logic
  // };

  // --- Core Measurements (PM, Temp, Hum) ---
  // PM values are unsigned, 0xFFFF is the invalid marker
  float pm1 = (m[0] == 0xFFFF) ? NAN : m[0] / 10.0f;
  float pm2_5 = (m[1] == 0xFFFF) ? NAN : m[1] / 10.0f;
  float pm4 = (m[2] == 0xFFFF) ? NAN : m[2] / 10.0f;
  float pm10 = (m[3] == 0xFFFF) ? NAN : m[3] / 10.0f;

  // Temperature and Humidity are signed int16, 0x7FFF is the invalid marker
  float hum = (m[4] == 0x7FFF) ? NAN : static_cast<int16_t>(m[4]) / 100.0f;
  float temp = (m[5] == 0x7FFF) ? NAN : static_cast<int16_t>(m[5]) / 200.0f;

  // --- Dynamic Index Mapping based on model ---
  int8_t voc_idx = -1, nox_idx = -1, hcho_idx = -1, co2_idx = -1;
  bool co2_is_u16 = false;

  switch (this->sen6x_type_) {
    case SEN63C:
      co2_idx = 6;
      break;
    case SEN65:
      voc_idx = 6;
      nox_idx = 7;
      break;
    case SEN66:
      voc_idx = 6;
      nox_idx = 7;
      co2_idx = 8;
      co2_is_u16 = true;
      break;
    case SEN68:
      voc_idx = 6;
      nox_idx = 7;
      hcho_idx = 8;
      break;
    case SEN69C:
      voc_idx = 6;
      nox_idx = 7;
      hcho_idx = 8;
      co2_idx = 9;
      break;
    default:
      break;
  }

  // --- Gas Index Measurements ---
  // VOC/NOx are only valid after warmup (state == MEASURING)
  float voc = NAN, nox = NAN, hcho = NAN, co2 = NAN;

  if (this->state_ == MEASURING) {
    if (voc_idx >= 0)
      voc = (m[voc_idx] == 0x7FFF) ? NAN : static_cast<int16_t>(m[voc_idx]) / 10.0f;
    if (nox_idx >= 0)
      nox = (m[nox_idx] == 0x7FFF) ? NAN : static_cast<int16_t>(m[nox_idx]) / 10.0f;
  }

  if (hcho_idx >= 0)
    hcho = (m[hcho_idx] == 0xFFFF) ? NAN : m[hcho_idx] / 10.0f;

  if (co2_idx >= 0) {
    if (co2_is_u16)
      co2 = (m[co2_idx] == 0xFFFF) ? NAN : m[co2_idx];
    else
      co2 = (m[co2_idx] == 0x7FFF) ? NAN : static_cast<int16_t>(m[co2_idx]);
  }

  // --- Publish States ---
  if (this->pm_1_0_sensor_)
    this->pm_1_0_sensor_->publish_state(pm1);
  if (this->pm_2_5_sensor_)
    this->pm_2_5_sensor_->publish_state(pm2_5);
  if (this->pm_4_0_sensor_)
    this->pm_4_0_sensor_->publish_state(pm4);
  if (this->pm_10_0_sensor_)
    this->pm_10_0_sensor_->publish_state(pm10);
  if (this->temperature_sensor_)
    this->temperature_sensor_->publish_state(temp);
  if (this->humidity_sensor_)
    this->humidity_sensor_->publish_state(hum);
  if (this->voc_sensor_)
    this->voc_sensor_->publish_state(voc);
  if (this->nox_sensor_)
    this->nox_sensor_->publish_state(nox);
  if (this->hcho_sensor_)
    this->hcho_sensor_->publish_state(hcho);
  if (this->co2_sensor_)
    this->co2_sensor_->publish_state(co2);

  // --- Handle Number Concentration (NC) ---
  const bool has_nc_configured = (this->pm_nc_0_5_sensor_ || this->pm_nc_1_0_sensor_ || this->pm_nc_2_5_sensor_);
  const bool can_read_nc = (this->sen6x_type_ != SEN69C);  // SEN69C usually doesn't provide NC via 0x0316

  if (has_nc_configured && can_read_nc) {
    this->read_number_concentration_();
  }
}

void SEN6XComponent::read_number_concentration_() {
  // Step 1: Request Number Concentration data from the sensor
  // This command is specific to models that support particle counting (NC)
  if (!this->write_command(SEN6X_CMD_READ_NUMBER_CONCENTRATION)) {
    ESP_LOGE(TAG, "Failed to send read number concentration command! (Error: %d)", this->last_error_);
    return;
  }
  // Step 2: Wait for the sensor to prepare the data buffer
  // Then proceed to parse and publish the NC values
  this->set_timeout(TIMEOUT_POLL, I2C_READ_DELAY, [this]() { this->parse_and_publish_number_concentration_(); });
}

void SEN6XComponent::parse_and_publish_number_concentration_() {
  uint16_t m[5];  // NC 0.5, 1.0, 2.5, 4.0, 10.0

  // Read the 5 data words for Number Concentration
  if (!this->read_data(m, 5)) {
    ESP_LOGE(TAG, "Failed to read number concentration data! (Error: %d)", this->last_error_);
    return;
  }

  // Parse values: Sensirion uses 0xFFFF as an invalid/missing data marker.
  // Valid values must be divided by 10.0 to get the correct concentration.
  float nc0_5 = (m[0] == 0xFFFF) ? NAN : m[0] / 10.0f;
  float nc1_0 = (m[1] == 0xFFFF) ? NAN : m[1] / 10.0f;
  float nc2_5 = (m[2] == 0xFFFF) ? NAN : m[2] / 10.0f;
  float nc4_0 = (m[3] == 0xFFFF) ? NAN : m[3] / 10.0f;
  float nc10_0 = (m[4] == 0xFFFF) ? NAN : m[4] / 10.0f;

  // Publish states to ESPHome sensors if they are configured
  if (this->pm_nc_0_5_sensor_)
    this->pm_nc_0_5_sensor_->publish_state(nc0_5);
  if (this->pm_nc_1_0_sensor_)
    this->pm_nc_1_0_sensor_->publish_state(nc1_0);
  if (this->pm_nc_2_5_sensor_)
    this->pm_nc_2_5_sensor_->publish_state(nc2_5);
  if (this->pm_nc_4_0_sensor_)
    this->pm_nc_4_0_sensor_->publish_state(nc4_0);
  if (this->pm_nc_10_0_sensor_)
    this->pm_nc_10_0_sensor_->publish_state(nc10_0);

  // Success: ensure any transient communication warnings are cleared
  this->status_clear_warning();
}

void SEN6XComponent::stop_measurement() {
  // Prevent stopping if the sensor is already IDLE or not yet fully initialized
  if (!this->is_measuring_()) {
    ESP_LOGW(TAG, "Cannot stop measurement: sensor is not in a measuring state");
    return;
  }

  // Attempt to send the I2C stop command
  if (!this->write_command(SEN6X_CMD_STOP_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Failed to stop measurement!");
    return;
  }

  // Immediately cancel any pending polling or state transition timeouts
  this->cancel_timeout(TIMEOUT_POLL);
  this->state_ = IDLE;

  ESP_LOGI(TAG, "Measurement stopped");
}

void SEN6XComponent::start_continuous_measurement() {
  // Only allow starting if the sensor is explicitly in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot start measurement: sensor is already active");
    return;
  }

  // Attempt to send the I2C start command
  if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Failed to start continuous measurement!");
    return;
  }

  // Ensure any previous polling chain is cleared before starting a new one
  this->cancel_timeout(TIMEOUT_POLL);

  // Sequence: Wait for command processing -> WARMING_UP -> MEASURING
  this->set_timeout(TIMEOUT_POLL, STARTUP_TIME, [this]() {
    this->state_ = WARMING_UP;
    ESP_LOGD(TAG, "Sensor is warming up...");

    this->set_timeout(TIMEOUT_POLL, WARMUP_TIME, [this]() {
      this->state_ = MEASURING;
      ESP_LOGD(TAG, "Sensor is now in full measuring mode");
    });
  });

  ESP_LOGI(TAG, "Continuous measurement started");
}

bool SEN6XComponent::get_data_ready() {
  // Check if the sensor is in a valid state to produce data
  if (!this->is_measuring_()) {
    ESP_LOGW(TAG, "Cannot check Data Ready: sensor is not measuring");
    return false;
  }

  uint16_t status_reg;
  // Use get_register to handle the write command, delay, and read sequence
  if (!this->get_register(SEN6X_CMD_GET_DATA_READY_STATUS, status_reg, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read Data Ready status!");
    return false;
  }

  // Bit 0: 1 indicates data is ready to be read, 0 indicates busy
  return (status_reg & 0x0001) != 0;
}

void SEN6XComponent::device_reset() {
  ESP_LOGI(TAG, "Initiating soft reset...");

  // Cancel any active polling or state transition timers before resetting
  this->cancel_timeout(TIMEOUT_POLL);

  if (!this->write_command(SEN6X_CMD_RESET)) {
    ESP_LOGE(TAG, "Failed to send reset command!");
    return;
  }

  this->state_ = STARTING;

  // After reset, the sensor needs time to reboot (INIT_TIME)
  this->set_timeout(TIMEOUT_POLL, INIT_TIME, [this]() {
    ESP_LOGD(TAG, "Sensor rebooted, restarting measurements...");

    if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
      ESP_LOGE(TAG, "Failed to restart measurements after reset!");
      this->mark_failed();
      return;
    }

    this->state_ = WARMING_UP;
    ESP_LOGD(TAG, "Sensor is warming up...");

    // Wait for the algorithm to stabilize (WARMUP_TIME)
    this->set_timeout(TIMEOUT_POLL, WARMUP_TIME, [this]() {
      this->state_ = MEASURING;
      ESP_LOGD(TAG, "Sensor is now measuring");
    });
  });
}

uint32_t SEN6XComponent::get_device_status(bool clear_after_read) {
  // Select the appropriate command based on the clear_after_read flag
  uint16_t cmd = clear_after_read ? SEN6X_CMD_READ_AND_CLEAR_DEVICE_STATUS : SEN6X_CMD_READ_DEVICE_STATUS;
  uint16_t raw_status[2];

  // Read the two 16-bit words that compose the 32-bit status register
  if (!this->get_register(cmd, raw_status, 2, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read device status register!");
    return 0;
  }

  // Combine the two words into a single 32-bit unsigned integer
  uint32_t status = (static_cast<uint32_t>(raw_status[0]) << 16) | raw_status[1];

  ESP_LOGD(TAG, "Device Status Register: 0x%08X", status);
  return status;
}

void SEN6XComponent::set_temperature_offset(float offset_c) {
  // Sensirion expects the offset in degrees Celsius multiplied by 200.
  // We cast to int16_t first to handle potential negative offsets correctly
  // before storing in the uint16_t register format.
  int16_t offset_val = static_cast<int16_t>(offset_c * 200.0f);

  // Attempt to write the offset to the sensor's non-volatile memory or shadow register
  if (!this->write_command(SEN6X_CMD_SET_TEMPERATURE_OFFSET, static_cast<uint16_t>(offset_val))) {
    ESP_LOGE(TAG, "Failed to set temperature offset!");
    return;
  }

  ESP_LOGI(TAG, "Temperature offset successfully set to %.2f °C", offset_c);
}

void SEN6XComponent::set_temperature_acceleration(uint16_t profile) {
  // This setting can only be changed when the sensor is not actively measuring
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot set temperature acceleration: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return;
  }

  // Valid profiles: 0 (Default), 1 (Slow), 2 (Fast)
  if (profile > 2) {
    ESP_LOGE(TAG, "Invalid temperature acceleration profile: %u (allowed: 0, 1, 2)", profile);
    return;
  }

  // Attempt to write the profile to the sensor register
  if (!this->write_command(SEN6X_CMD_SET_TEMPERATURE_ACCELERATION, profile)) {
    ESP_LOGE(TAG, "Failed to set temperature acceleration profile!");
    return;
  }

  ESP_LOGI(TAG, "Temperature acceleration profile successfully set to: %u", profile);
}

void SEN6XComponent::set_ambient_pressure(uint16_t pressure_hpa) {
  // Ambient pressure compensation is only effective during active measurements
  if (!this->is_measuring_()) {
    ESP_LOGW(TAG, "Cannot set ambient pressure: sensor is not in a measuring state");
    return;
  }

  // Only models with CO2 sensors (SEN63C, SEN66, SEN69C) support pressure compensation
  const bool supports_pressure =
      (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_pressure) {
    ESP_LOGW(TAG, "Ambient pressure compensation is not supported on this model (%s)", this->product_name_.c_str());
    return;
  }

  // Attempt to write the pressure value (in hPa) to the sensor
  if (!this->write_command(SEN6X_CMD_SET_AMBIENT_PRESSURE, pressure_hpa)) {
    ESP_LOGE(TAG, "Failed to set ambient pressure!");
    return;
  }

  ESP_LOGI(TAG, "Ambient pressure compensation set to: %u hPa", pressure_hpa);
}

uint16_t SEN6XComponent::get_ambient_pressure() {
  // Only models with CO2 sensors (SEN63C, SEN66, SEN69C) support pressure readings
  const bool supports_pressure =
      (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_pressure) {
    ESP_LOGW(TAG, "Ambient pressure reading is not supported on this model (%s)", this->product_name_.c_str());
    return 0;
  }

  uint16_t pressure_hpa = 0;
  // Request the current ambient pressure compensation value from the sensor
  if (!this->get_register(SEN6X_CMD_GET_AMBIENT_PRESSURE, pressure_hpa, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read ambient pressure!");
    return 0;
  }

  ESP_LOGD(TAG, "Sensor reported ambient pressure: %u hPa", pressure_hpa);
  return pressure_hpa;
}

void SEN6XComponent::set_sensor_altitude(uint16_t altitude_meters) {
  // Sensor altitude can only be configured while the sensor is in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot set altitude: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return;
  }

  // Only models with CO2 sensors (SEN63C, SEN66, SEN69C) support altitude compensation
  const bool supports_altitude =
      (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_altitude) {
    ESP_LOGW(TAG, "Altitude compensation is not supported on this model (%s)", this->product_name_.c_str());
    return;
  }

  // Attempt to write the altitude (in meters) to the sensor register
  if (!this->write_command(SEN6X_CMD_SET_SENSOR_ALTITUDE, altitude_meters)) {
    ESP_LOGE(TAG, "Failed to set sensor altitude!");
    return;
  }

  ESP_LOGI(TAG, "Sensor altitude compensation set to: %u meters", altitude_meters);
}

uint16_t SEN6XComponent::get_sensor_altitude() {
  // Only models with CO2 sensors (SEN63C, SEN66, SEN69C) support altitude compensation
  const bool supports_altitude =
      (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_altitude) {
    ESP_LOGW(TAG, "Altitude reading is not supported on this model (%s)", this->product_name_.c_str());
    return 0;
  }

  uint16_t altitude = 0;
  // Request the current altitude compensation value (in meters) from the sensor
  if (!this->get_register(SEN6X_CMD_GET_SENSOR_ALTITUDE, altitude, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read sensor altitude!");
    return 0;
  }

  ESP_LOGD(TAG, "Sensor reported altitude: %u m", altitude);
  return altitude;
}

void SEN6XComponent::start_fan_cleaning() {
  // Fan cleaning is a high-speed burst that must be triggered in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot start fan cleaning: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return;
  }

  // Attempt to send the fan cleaning command (accelerates fan to maximum speed)
  if (!this->write_command(SEN6X_CMD_START_FAN_CLEANING)) {
    ESP_LOGE(TAG, "Failed to start fan cleaning!");
    return;
  }

  ESP_LOGI(TAG, "Fan cleaning process started successfully");
}

void SEN6XComponent::activate_sht_heater() {
  // The heater is used to remove condensation and must be activated in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot activate SHT heater: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return;
  }

  // Attempt to send the command to activate the internal SHT heater
  if (!this->write_command(SEN6X_CMD_ACTIVATE_SHT_HEATER)) {
    ESP_LOGE(TAG, "Failed to activate SHT heater!");
    return;
  }

  ESP_LOGI(TAG, "SHT heater successfully activated");
}

bool SEN6XComponent::get_sht_heater_measurements(float &temp, float &hum) {
  // SHT Heater measurements can only be retrieved while the sensor is in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot read SHT heater measurements: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return false;
  }

  uint16_t data[3];  // Response format: [0]=Temperature, [1]=Humidity, [2]=Dummy/Reserved

  // Request the specific heater-influenced RHT data from the sensor
  if (!this->get_register(SEN6X_CMD_GET_SHT_HEATER_MEASUREMENTS, data, 3, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read SHT heater measurements!");
    return false;
  }

  // Parse values with Sensirion's 0x7FFF invalid data marker check
  // Temperature: ticks / 200.0, Humidity: ticks / 100.0
  temp = (data[0] == 0x7FFF) ? NAN : static_cast<int16_t>(data[0]) / 200.0f;
  hum = (data[1] == 0x7FFF) ? NAN : static_cast<int16_t>(data[1]) / 100.0f;

  ESP_LOGV(TAG, "SHT heater diagnostics: T=%.2f °C, RH=%.2f %%", temp, hum);
  return true;
}

void SEN6XComponent::set_voc_algorithm_tuning_parameters(uint16_t index_offset, uint16_t learning_time, uint16_t gain,
                                                         uint16_t gate_max) {
  // Tuning parameters for the gas engine must be configured in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot set VOC tuning: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return;
  }

  // Only models equipped with a VOC sensor support algorithm tuning
  const bool supports_voc = (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN68 ||
                             this->sen6x_type_ == SEN69C);

  if (!supports_voc) {
    ESP_LOGW(TAG, "VOC algorithm tuning is not supported on this model (%s)", this->product_name_.c_str());
    return;
  }

  // Prepare the 4-word data packet for the Sensirion Gas Index Algorithm
  uint16_t data[4] = {index_offset, learning_time, gain, gate_max};

  if (!this->write_command(SEN6X_CMD_SET_VOC_ALGORITHM_TUNING_PARAMETERS, data, 4)) {
    ESP_LOGE(TAG, "Failed to set VOC algorithm tuning parameters!");
    return;
  }

  ESP_LOGI(TAG, "VOC tuning successfully set: Offset=%u, Learning=%u h, Gain=%u, Gate=%u m", index_offset,
           learning_time, gain, gate_max);
}

void SEN6XComponent::set_nox_algorithm_tuning_parameters(uint16_t index_offset, uint16_t learning_time, uint16_t gain,
                                                         uint16_t gate_max) {
  // NOx algorithm parameters can only be updated while the sensor is in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot set NOx tuning: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return;
  }

  // Only models equipped with a NOx sensor support algorithm tuning
  const bool supports_nox = (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN68 ||
                             this->sen6x_type_ == SEN69C);

  if (!supports_nox) {
    ESP_LOGW(TAG, "NOx algorithm tuning is not supported on this model (%s)", this->product_name_.c_str());
    return;
  }

  // Pack the 4 tuning words for the Sensirion NOx Index Algorithm
  uint16_t data[4] = {index_offset, learning_time, gain, gate_max};

  if (!this->write_command(SEN6X_CMD_SET_NOX_ALGORITHM_TUNING_PARAMETERS, data, 4)) {
    ESP_LOGE(TAG, "Failed to set NOx algorithm tuning parameters!");
    return;
  }

  ESP_LOGI(TAG, "NOx tuning successfully set: Offset=%u, Learning=%u h, Gain=%u, Gate=%u m", index_offset,
           learning_time, gain, gate_max);
}

bool SEN6XComponent::get_voc_algorithm_tuning_parameters(uint16_t &index_offset, uint16_t &learning_time,
                                                         uint16_t &gain, uint16_t &gate_max) {
  // Tuning parameters can only be retrieved while the sensor is in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot get VOC tuning: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return false;
  }

  // Only models with gas sensors (VOC/NOx) support these parameters
  const bool supports_voc = (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN68 ||
                             this->sen6x_type_ == SEN69C);

  if (!supports_voc) {
    ESP_LOGW(TAG, "VOC tuning is not supported on this model (%s)", this->product_name_.c_str());
    return false;
  }

  uint16_t data[4];
  // Request the 4-word tuning packet from the sensor
  if (!this->get_register(SEN6X_CMD_GET_VOC_ALGORITHM_TUNING_PARAMETERS, data, 4, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read VOC algorithm tuning parameters!");
    return false;
  }

  index_offset = data[0];
  learning_time = data[1];
  gain = data[2];
  gate_max = data[3];

  ESP_LOGI(TAG, "VOC tuning parameters read: Offset=%u, Learning=%u h, Gain=%u, Gate=%u m", index_offset, learning_time,
           gain, gate_max);
  return true;
}

bool SEN6XComponent::get_nox_algorithm_tuning_parameters(uint16_t &index_offset, uint16_t &learning_time,
                                                         uint16_t &gain, uint16_t &gate_max) {
  // Tuning parameters can only be retrieved while the sensor is in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot get NOx tuning: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return false;
  }

  const bool supports_nox = (this->sen6x_type_ == SEN65 || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN68 ||
                             this->sen6x_type_ == SEN69C);

  if (!supports_nox) {
    ESP_LOGW(TAG, "NOx tuning is not supported on this model (%s)", this->product_name_.c_str());
    return false;
  }

  uint16_t data[4];
  // Request the 4-word tuning packet from the sensor
  if (!this->get_register(SEN6X_CMD_GET_NOX_ALGORITHM_TUNING_PARAMETERS, data, 4, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read NOx algorithm tuning parameters!");
    return false;
  }

  index_offset = data[0];
  learning_time = data[1];
  gain = data[2];
  gate_max = data[3];

  ESP_LOGI(TAG, "NOx tuning parameters read: Offset=%u, Learning=%u h, Gain=%u, Gate=%u m", index_offset, learning_time,
           gain, gate_max);
  return true;
}

void SEN6XComponent::set_co2_automatic_self_calibration(bool enable) {
  // Check if the current model supports CO2 sensor features (ASC)
  const bool supports_co2 = (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_co2) {
    ESP_LOGW(TAG, "CO2 Automatic Self Calibration is not supported on this model (%s)", this->product_name_.c_str());
    return;
  }

  // Convert boolean to the 16-bit value expected by the sensor (1=ON, 0=OFF)
  uint16_t value = enable ? 0x0001 : 0x0000;

  // Attempt to write the ASC setting to the sensor's non-volatile memory
  if (!this->write_command(SEN6X_CMD_SET_CO2_SENSOR_AUTOMATIC_SELF_CALIBRATION, value)) {
    ESP_LOGE(TAG, "Failed to set CO2 Automatic Self Calibration!");
    return;
  }

  ESP_LOGI(TAG, "CO2 Automatic Self Calibration successfully set to: %s", enable ? "ENABLED" : "DISABLED");
}

bool SEN6XComponent::get_co2_automatic_self_calibration() {
  // Only models with CO2 sensors (SEN63C, SEN66, SEN69C) support ASC
  const bool supports_co2 = (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_co2) {
    ESP_LOGW(TAG, "CO2 Automatic Self Calibration reading is not supported on this model (%s)",
             this->product_name_.c_str());
    return false;
  }

  uint16_t value = 0;
  // Request the current ASC status (1 = Enabled, 0 = Disabled)
  if (!this->get_register(SEN6X_CMD_GET_CO2_SENSOR_AUTOMATIC_SELF_CALIBRATION, value, I2C_READ_DELAY)) {
    ESP_LOGE(TAG, "Failed to read CO2 Automatic Self Calibration status!");
    return false;
  }

  ESP_LOGD(TAG, "CO2 Automatic Self Calibration status: %s", (value == 0x0001) ? "ENABLED" : "DISABLED");
  return (value == 0x0001);
}

void SEN6XComponent::perform_forced_co2_recalibration(uint16_t co2_ppm) {
  // Only models with CO2 sensors (SEN63C, SEN66, SEN69C) support FRC
  const bool supports_co2 = (this->sen6x_type_ == SEN63C || this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_co2) {
    ESP_LOGW(TAG, "Forced CO2 Recalibration is not supported on this model (%s)", this->product_name_.c_str());
    return;
  }

  // FRC requires the sensor to be actively measuring for at least several minutes
  if (!this->is_measuring_()) {
    ESP_LOGW(TAG, "Cannot perform FRC: sensor must be in MEASURING state (current: %s)", this->get_state().c_str());
    return;
  }

  // Attempt to send the reference CO2 concentration (in ppm)
  if (!this->write_command(SEN6X_CMD_SET_PERFORM_FORCED_CO2_RECALIBRATION, co2_ppm)) {
    ESP_LOGE(TAG, "Failed to perform Forced CO2 Recalibration!");
    return;
  }

  ESP_LOGI(TAG, "Forced CO2 Recalibration successful at %u ppm", co2_ppm);
}

void SEN6XComponent::perform_co2_sensor_factory_reset() {
  // CO2 factory reset is only applicable to specific CO2-enabled models
  const bool supports_co2_reset = (this->sen6x_type_ == SEN66 || this->sen6x_type_ == SEN69C);

  if (!supports_co2_reset) {
    ESP_LOGW(TAG, "CO2 factory reset is not supported on this model (%s)", this->product_name_.c_str());
    return;
  }

  // It is recommended to perform NVM (Non-Volatile Memory) operations in IDLE state
  if (!this->is_idle_()) {
    ESP_LOGW(TAG, "Cannot perform CO2 factory reset: sensor is not IDLE (current: %s)", this->get_state().c_str());
    return;
  }

  // Attempt to send the factory reset command for the CO2 sub-module
  if (!this->write_command(SEN6X_CMD_SET_PERFORM_CO2_SENSOR_FACTORY_RESET)) {
    ESP_LOGE(TAG, "Failed to perform CO2 factory reset!");
    return;
  }

  ESP_LOGI(TAG, "CO2 factory reset successfully executed");
}

}  // namespace esphome::sen6x

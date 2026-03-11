#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::sen6x {

static const char *const TAG = "sen6x";

static constexpr uint8_t POLL_RETRIES = 24;     // 24 attempts
static constexpr uint32_t I2C_READ_DELAY = 20;  // 20 ms to wait for I2C read to complete
static constexpr uint32_t POLL_INTERVAL = 50;   // 50 ms between poll attempts
// Single numeric timeout ID — the chain is sequential so only one is active at a time.
static constexpr uint32_t TIMEOUT_POLL = 1;
static constexpr uint16_t SEN6X_CMD_GET_DATA_READY_STATUS = 0x0202;
static constexpr uint16_t SEN6X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static constexpr uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static constexpr uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;

static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT = 0x0300;  // SEN66 only!
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN62 = 0x04A3;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN63C = 0x0471;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN65 = 0x0446;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN68 = 0x0467;
static constexpr uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN69C = 0x04B5;

static constexpr uint16_t SEN6X_CMD_START_MEASUREMENTS = 0x0021;
static constexpr uint16_t SEN6X_CMD_RESET = 0xD304;

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

      // Step 2: Read product name in next loop iteration
      this->set_timeout(0, [this]() {
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

          if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
            ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
            this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
            return;
          }

          this->set_timeout(60000, [this]() { this->startup_complete_ = true; });
          this->initialized_ = true;
          ESP_LOGD(TAG, "Initialized");
        });
      });
    });
  });
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

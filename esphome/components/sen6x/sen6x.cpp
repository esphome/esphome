#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cinttypes>
#include <functional>
#include <memory>

namespace esphome::sen6x {

static const char *const TAG = "sen6x";

static const uint16_t SEN6X_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SEN6X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static const uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static const uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;

static const uint16_t SEN6X_CMD_READ_MEASUREMENT = 0x0300;  // SEN66 only!
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN62 = 0x04A3;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN63C = 0x0471;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN65 = 0x0446;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN68 = 0x0467;
static const uint16_t SEN6X_CMD_READ_MEASUREMENT_SEN69C = 0x04B5;

static const uint16_t SEN6X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN6X_CMD_RESET = 0xD304;

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
      ESP_LOGE(TAG, "Failed to reset sensor");
      this->mark_failed();
      return;
    }

    // After reset the sensor needs 100 ms to become ready
    this->set_timeout(100, [this]() {
      uint16_t raw_serial_number[16];
      if (!this->get_register(SEN6X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 16, 20)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      this->serial_number_.clear();
      this->serial_number_.reserve(32);
      for (const uint16_t word : raw_serial_number) {
        const char c1 = static_cast<char>(word >> 8);
        const char c2 = static_cast<char>(word & 0xFF);
        if (c1 == '\0')
          break;
        this->serial_number_.push_back(c1);
        if (c2 == '\0')
          break;
        this->serial_number_.push_back(c2);
      }
      ESP_LOGD(TAG, "Serial number %s", this->serial_number_.c_str());

      uint16_t raw_product_name[16];
      if (!this->get_register(SEN6X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
        ESP_LOGE(TAG, "Failed to read product name");
        this->error_code_ = PRODUCT_NAME_FAILED;
        this->mark_failed();
        return;
      }

      this->product_name_.clear();
      for (const uint16_t word : raw_product_name) {
        const char c1 = static_cast<char>(word >> 8);
        const char c2 = static_cast<char>(word & 0xFF);
        if (c1 == '\0')
          break;
        this->product_name_.push_back(c1);
        if (c2 == '\0')
          break;
        this->product_name_.push_back(c2);
      }

      Sen6xType inferred_type = this->infer_type_from_product_name_(this->product_name_);
      if (this->sen6x_type_ == UNKNOWN) {
        this->sen6x_type_ = inferred_type;
        if (inferred_type == UNKNOWN) {
          ESP_LOGE(TAG, "Unable to infer sensor type from product name '%s'. Please specify 'type' in configuration.",
                   this->product_name_.c_str());
          this->error_code_ = PRODUCT_NAME_FAILED;
          this->mark_failed();
          return;
        }
        ESP_LOGD(TAG, "Sensor type inferred from product name: %s", this->product_name_.c_str());
      } else if (this->sen6x_type_ != inferred_type && inferred_type != UNKNOWN) {
        ESP_LOGW(TAG,
                 "Configured sensor type does not match product name '%s'. "
                 "Using configured type, but this may cause issues.",
                 this->product_name_.c_str());
      }
      ESP_LOGD(TAG, "Product name: %s", this->product_name_.c_str());

      uint16_t raw_firmware_version = 0;
      if (!this->get_register(SEN6X_CMD_GET_FIRMWARE_VERSION, raw_firmware_version, 20)) {
        ESP_LOGE(TAG, "Failed to read firmware version");
        this->error_code_ = FIRMWARE_FAILED;
        this->mark_failed();
        return;
      }
      this->firmware_version_major_ = (raw_firmware_version >> 8) & 0xFF;
      this->firmware_version_minor_ = raw_firmware_version & 0xFF;
      ESP_LOGD(TAG, "Firmware version %u.%u", this->firmware_version_major_, this->firmware_version_minor_);

      if (!this->write_command(SEN6X_CMD_START_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }

      this->startup_stable_after_ = App.get_loop_component_start_time() + 60000;
      this->initialized_ = true;
      ESP_LOGD(TAG, "Sensor initialized");
    });
  });
}

void SEN6XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "sen6x:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Product: %s", this->product_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Serial: %s", this->serial_number_.c_str());
  ESP_LOGCONFIG(TAG, "  Firmware: %u.%u", this->firmware_version_major_, this->firmware_version_minor_);

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

  uint16_t read_cmd;
  uint8_t read_words;
  set_read_command_and_words(this->sen6x_type_, read_cmd, read_words);

  const uint8_t poll_retries = 24;
  auto poll_ready = std::make_shared<std::function<void(uint8_t)>>();
  *poll_ready = [this, poll_ready, read_cmd, read_words](uint8_t retries_left) {
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
      this->set_timeout(50, [poll_ready, retries_left]() { (*poll_ready)(retries_left - 1); });
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

      const uint32_t now = App.get_loop_component_start_time();
      if (now < this->startup_stable_after_) {
        ESP_LOGD(TAG, "Startup delay active, ignoring sensor values");
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

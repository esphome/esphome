#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "sps30.h"

namespace esphome {
namespace sps30 {

static const char *const TAG = "sps30";

static const uint16_t SPS30_CMD_GET_ARTICLE_CODE = 0xD025;
static const uint16_t SPS30_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SPS30_CMD_GET_FIRMWARE_VERSION = 0xD100;
static const uint16_t SPS30_CMD_START_CONTINUOUS_MEASUREMENTS = 0x0010;
static const uint16_t SPS30_CMD_START_CONTINUOUS_MEASUREMENTS_ARG = 0x0300;
static const uint16_t SPS30_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SPS30_CMD_READ_MEASUREMENT = 0x0300;
static const uint16_t SPS30_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SPS30_CMD_SET_AUTOMATIC_CLEANING_INTERVAL_SECONDS = 0x8004;
static const uint16_t SPS30_CMD_START_FAN_CLEANING = 0x5607;
static const uint16_t SPS30_CMD_SOFT_RESET = 0xD304;
static const size_t SERIAL_NUMBER_LENGTH = 8;
static const uint8_t MAX_SKIPPED_DATA_CYCLES_BEFORE_ERROR = 5;

void SPS30Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sps30...");
  this->write_command(SPS30_CMD_SOFT_RESET);
  /// Deferred Sensor initialization
  this->set_timeout(500, [this]() {
    /// Firmware version identification
    if (!this->get_register(SPS30_CMD_GET_FIRMWARE_VERSION, raw_firmware_version_, 1)) {
      this->error_code_ = FIRMWARE_VERSION_READ_FAILED;
      this->mark_failed();
      return;
    }
    /// Serial number identification
    uint16_t raw_serial_number[8];
    if (!this->get_register(SPS30_CMD_GET_SERIAL_NUMBER, raw_serial_number, 8, 1)) {
      this->error_code_ = SERIAL_NUMBER_READ_FAILED;
      this->mark_failed();
      return;
    }

    for (size_t i = 0; i < 8; ++i) {
      this->serial_number_[i * 2] = static_cast<char>(raw_serial_number[i] >> 8);
      this->serial_number_[i * 2 + 1] = uint16_t(uint16_t(raw_serial_number[i] & 0xFF));
    }
    ESP_LOGD(TAG, "  Serial Number: '%s'", this->serial_number_);

    bool result;
    if (this->fan_interval_.has_value()) {
      // override default value
      result = write_command(SPS30_CMD_SET_AUTOMATIC_CLEANING_INTERVAL_SECONDS, this->fan_interval_.value());
    } else {
      result = write_command(SPS30_CMD_SET_AUTOMATIC_CLEANING_INTERVAL_SECONDS);
    }
    if (result) {
      delay(20);
      uint16_t secs[2];
      if (this->read_data(secs, 2)) {
        fan_interval_ = secs[0] << 16 | secs[1];
      }
    }

    this->status_clear_warning();
    this->skipped_data_read_cycles_ = 0;
    this->start_continuous_measurement_();
  });
}

void SPS30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "sps30:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement Initialization failed!");
        break;
      case SERIAL_NUMBER_REQUEST_FAILED:
        ESP_LOGW(TAG, "Unable to request sensor serial number");
        break;
      case SERIAL_NUMBER_READ_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor serial number");
        break;
      case FIRMWARE_VERSION_REQUEST_FAILED:
        ESP_LOGW(TAG, "Unable to request sensor firmware version");
        break;
      case FIRMWARE_VERSION_READ_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Serial Number: '%s'", this->serial_number_);
  ESP_LOGCONFIG(TAG, "  Firmware version v%0d.%0d", (raw_firmware_version_ >> 8),
                uint16_t(raw_firmware_version_ & 0xFF));
  LOG_SENSOR("  ", "PM1.0 Weight Concentration", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5 Weight Concentration", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM4 Weight Concentration", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM10 Weight Concentration", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "PM1.0 Number Concentration", this->pmc_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5 Number Concentration", this->pmc_2_5_sensor_);
  LOG_SENSOR("  ", "PM4 Number Concentration", this->pmc_4_0_sensor_);
  LOG_SENSOR("  ", "PM10 Number Concentration", this->pmc_10_0_sensor_);
  LOG_SENSOR("  ", "PM typical size", this->pm_size_sensor_);
}

void SPS30Component::update() {
  /// Check if warning flag active (sensor reconnected?)
  if (this->status_has_warning()) {
    ESP_LOGD(TAG, "Trying to reconnect the sensor...");
    if (this->write_command(SPS30_CMD_SOFT_RESET)) {
      ESP_LOGD(TAG, "Sensor has soft-reset successfully. Waiting for reconnection in 500ms...");
      this->set_timeout(500, [this]() {
        this->start_continuous_measurement_();
        /// Sensor restarted and reading attempt made next cycle
        this->status_clear_warning();
        this->skipped_data_read_cycles_ = 0;
        ESP_LOGD(TAG, "Sensor reconnected successfully. Resuming continuous measurement!");
      });
    } else {
      ESP_LOGD(TAG, "Sensor soft-reset failed. Is the sensor offline?");
    }
    return;
  }
  /// Check if measurement is ready before reading the value
  if (!this->write_command(SPS30_CMD_GET_DATA_READY_STATUS)) {
    this->status_set_warning();
    return;
  }

  uint16_t raw_read_status;
  if (!this->read_data(&raw_read_status, 1) || raw_read_status == 0x00) {
    ESP_LOGD(TAG, "Sensor measurement not ready yet.");
    this->skipped_data_read_cycles_++;
    /// The following logic is required to address the cases when a sensor is quickly replaced before it's marked
    /// as failed so that new sensor is eventually forced to be reinitialized for continuous measurement.
    if (this->skipped_data_read_cycles_ > MAX_SKIPPED_DATA_CYCLES_BEFORE_ERROR) {
      ESP_LOGD(TAG, "Sensor exceeded max allowed attempts. Sensor communication will be reinitialized.");
      this->status_set_warning();
    }
    return;
  }

  if (!this->write_command(SPS30_CMD_READ_MEASUREMENT)) {
    ESP_LOGW(TAG, "Error reading measurement status!");
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[20];
    if (!this->read_data(raw_data, 20)) {
      ESP_LOGW(TAG, "Error reading measurement data!");
      this->status_set_warning();
      return;
    }

    union uint32_float_t {
      uint32_t uint32;
      float value;
    };

    /// Reading and converting Mass concentration
    uint32_float_t pm_1_0{.uint32 = (((uint32_t(raw_data[0])) << 16) | (uint32_t(raw_data[1])))};
    uint32_float_t pm_2_5{.uint32 = (((uint32_t(raw_data[2])) << 16) | (uint32_t(raw_data[3])))};
    uint32_float_t pm_4_0{.uint32 = (((uint32_t(raw_data[4])) << 16) | (uint32_t(raw_data[5])))};
    uint32_float_t pm_10_0{.uint32 = (((uint32_t(raw_data[6])) << 16) | (uint32_t(raw_data[7])))};

    /// Reading and converting Number concentration
    uint32_float_t pmc_0_5{.uint32 = (((uint32_t(raw_data[8])) << 16) | (uint32_t(raw_data[9])))};
    uint32_float_t pmc_1_0{.uint32 = (((uint32_t(raw_data[10])) << 16) | (uint32_t(raw_data[11])))};
    uint32_float_t pmc_2_5{.uint32 = (((uint32_t(raw_data[12])) << 16) | (uint32_t(raw_data[13])))};
    uint32_float_t pmc_4_0{.uint32 = (((uint32_t(raw_data[14])) << 16) | (uint32_t(raw_data[15])))};
    uint32_float_t pmc_10_0{.uint32 = (((uint32_t(raw_data[16])) << 16) | (uint32_t(raw_data[17])))};

    /// Reading and converting Typical size
    uint32_float_t pm_size{.uint32 = (((uint32_t(raw_data[18])) << 16) | (uint32_t(raw_data[19])))};

    if (this->pm_1_0_sensor_ != nullptr)
      this->pm_1_0_sensor_->publish_state(pm_1_0.value);
    if (this->pm_2_5_sensor_ != nullptr)
      this->pm_2_5_sensor_->publish_state(pm_2_5.value);
    if (this->pm_4_0_sensor_ != nullptr)
      this->pm_4_0_sensor_->publish_state(pm_4_0.value);
    if (this->pm_10_0_sensor_ != nullptr)
      this->pm_10_0_sensor_->publish_state(pm_10_0.value);

    if (this->pmc_0_5_sensor_ != nullptr)
      this->pmc_0_5_sensor_->publish_state(pmc_0_5.value);
    if (this->pmc_1_0_sensor_ != nullptr)
      this->pmc_1_0_sensor_->publish_state(pmc_1_0.value);
    if (this->pmc_2_5_sensor_ != nullptr)
      this->pmc_2_5_sensor_->publish_state(pmc_2_5.value);
    if (this->pmc_4_0_sensor_ != nullptr)
      this->pmc_4_0_sensor_->publish_state(pmc_4_0.value);
    if (this->pmc_10_0_sensor_ != nullptr)
      this->pmc_10_0_sensor_->publish_state(pmc_10_0.value);

    if (this->pm_size_sensor_ != nullptr)
      this->pm_size_sensor_->publish_state(pm_size.value);

    this->status_clear_warning();
    this->skipped_data_read_cycles_ = 0;
  });
}

bool SPS30Component::start_continuous_measurement_() {
  uint8_t data[4];
  data[0] = SPS30_CMD_START_CONTINUOUS_MEASUREMENTS & 0xFF;
  data[1] = 0x03;
  data[2] = 0x00;
  data[3] = sht_crc_(0x03, 0x00);
  if (!this->write_command(SPS30_CMD_START_CONTINUOUS_MEASUREMENTS, SPS30_CMD_START_CONTINUOUS_MEASUREMENTS_ARG)) {
    ESP_LOGE(TAG, "Error initiating measurements");
    return false;
  }
  return true;
}

bool SPS30Component::start_fan_cleaning() {
  if (!write_command(SPS30_CMD_START_FAN_CLEANING)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start fan (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Fan auto clean started");
  }
  return true;
}

}  // namespace sps30
}  // namespace esphome

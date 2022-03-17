#include "sen5x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sen5x {

static const char *const TAG = "sen5x";

static const uint16_t SEN5X_CMD_START_MEASUREMENT = 0x0021;
static const uint16_t SEN5X_CMD_START_MEASUREMENT_RHT_GAS_ONLY = 0x0037;
static const uint16_t SEN5X_CMD_STOP_MEASUREMENT = 0x0104;
static const uint16_t SEN5X_CMD_READ_DATA_READY_FLAG = 0x0202;
static const uint16_t SEN5X_CMD_READ_MEASURED_VALUES = 0x03C4;
static const uint16_t SEN5X_CMD_READ_WRITE_T_COMPENSATION = 0x60B2;
static const uint16_t SEN5X_CMD_READ_WRITE_WARM_START_PARAMS = 0x60C6;
static const uint16_t SEN5X_CMD_READ_WRITE_VOC_ALGO_TUNING_PARAMS = 0x60D0;
static const uint16_t SEN5X_CMD_READ_WRITE_NOX_ALGO_TUNING_PARAMS = 0x60E1;
static const uint16_t SEN5X_CMD_READ_WRITE_RHT_ACCEL_MODE = 0x60F7;
static const uint16_t SEN5X_CMD_READ_WRITE_VOC_ALGO_STATE = 0x6181;
static const uint16_t SEN5X_CMD_START_FAN_CLEANING = 0x5607;
static const uint16_t SEN5X_CMD_READ_WRITE_AUTO_CLEANING_INTVL = 0x8004;
static const uint16_t SEN5X_CMD_READ_PRODUCT_NAME = 0xD014;
static const uint16_t SEN5X_CMD_READ_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN5X_CMD_READ_FIRMWARE_VERSION = 0xD100;
static const uint16_t SEN5X_CMD_READ_DEVICE_STATUS = 0xD206;
static const uint16_t SEN5X_CMD_CLEAR_DEVICE_STATUS = 0xD210;
static const uint16_t SEN5X_CMD_RESET = 0xD304;
static const size_t SERIAL_NUMBER_LENGTH = 32;  // number of ASCII Characters

static const uint8_t MAX_SKIPPED_DATA_CYCLES_BEFORE_ERROR = 5;

static const uint16_t UINT_INVALID = 0xFFFF;
static const int16_t INT_INVALID = 0x7FFF;

void SEN5xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen54...");
  this->write_command_(SEN5X_CMD_RESET);
  /// Deferred Sensor initialization
  this->set_timeout(500, [this]() {
    /// Firmware version identification
    if (!this->write_command_(SEN5X_CMD_READ_FIRMWARE_VERSION)) {
      this->error_code_ = FIRMWARE_VERSION_REQUEST_FAILED;
      this->mark_failed();
      return;
    }

    delay(20);

    if (!this->read_data_(&raw_firmware_version_, 1)) {
      this->error_code_ = FIRMWARE_VERSION_READ_FAILED;
      this->mark_failed();
      return;
    }

    /// Serial number identification
    if (!this->write_command_(SEN5X_CMD_READ_SERIAL_NUMBER)) {
      this->error_code_ = SERIAL_NUMBER_REQUEST_FAILED;
      this->mark_failed();
      return;
    }

    delay(20);

    uint16_t raw_serial_number[8];
    if (!this->read_data_(raw_serial_number, 8)) {
      this->error_code_ = SERIAL_NUMBER_READ_FAILED;
      this->mark_failed();
      return;
    }

    for (size_t i = 0; i < 8; ++i) {
      this->serial_number_[i * 2] = static_cast<char>(raw_serial_number[i] >> 8);
      this->serial_number_[i * 2 + 1] = uint16_t(uint16_t(raw_serial_number[i] & 0xFF));
    }
    ESP_LOGD(TAG, "  Serial Number: '%s'", this->serial_number_);
    this->status_clear_warning();
    this->skipped_data_read_cycles_ = 0;
    this->start_continuous_measurement_();

    initialized_ = true;
  });
}

void SEN5xComponent::dump_config() {
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
  LOG_SENSOR("  ", "Ambient Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Ambient Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "VOC Index", this->voc_sensor_);
}

void SEN5xComponent::update() {
  if (!initialized_) {
    return;
  }
  /// Check if warning flag active (sensor reconnected?)
  if (this->status_has_warning()) {
    ESP_LOGD(TAG, "Trying to reconnect the sensor...");
    if (this->write_command_(SEN5X_CMD_RESET)) {
      ESP_LOGD(TAG, "Sensor has soft-reset successfully. Waiting for reconnection in 1000ms...");
      this->set_timeout(1000, [this]() {
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
  if (!this->write_command_(SEN5X_CMD_READ_DATA_READY_FLAG)) {
    this->status_set_warning();
    return;
  }

  uint16_t raw_read_status;
  if (!this->read_data_(&raw_read_status, 1) || raw_read_status == 0x00) {
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

  if (!this->write_command_(SEN5X_CMD_READ_MEASURED_VALUES)) {
    ESP_LOGW(TAG, "Error reading measurement status!");
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[8];
    if (!this->read_data_(raw_data, 8)) {
      ESP_LOGW(TAG, "Error reading measurement data!");
      this->status_set_warning();
      return;
    }

    union uint32_float_t {
      uint32_t uint32;
      float value;
    };

    /// Reading and converting Mass concentration
    uint16_t pm_1_0_int = raw_data[0];
    uint16_t pm_2_5_int = raw_data[1];
    uint16_t pm_4_0_int = raw_data[2];
    uint16_t pm_10_0_int = raw_data[3];
    int16_t ambient_humi_int = static_cast<int16_t>(raw_data[4]);
    int16_t ambient_temp_int = static_cast<int16_t>(raw_data[5]);
    int16_t voc_index_int = static_cast<int16_t>(raw_data[6]);
    int16_t nox_index_int = static_cast<int16_t>(raw_data[7]);


    float pm_1_0 = pm_1_0_int == UINT_INVALID ? NAN : pm_1_0_int / 10.0f;
    float pm_2_5 = pm_2_5_int == UINT_INVALID ? NAN : pm_2_5_int / 10.0f;
    float pm_4_0 = pm_4_0_int == UINT_INVALID ? NAN : pm_4_0_int/ 10.0f;
    float pm_10_0 = pm_10_0_int == UINT_INVALID ? NAN : pm_10_0_int/ 10.0f;

    float ambient_humi = ambient_humi_int == INT_INVALID ? NAN : ambient_humi_int / 100.0f;
    float ambient_temp = ambient_temp_int == INT_INVALID ? NAN : ambient_temp_int / 200.0f;

    float voc_index = voc_index_int == INT_INVALID ? NAN : voc_index_int / 10.0f;
    float nox_index = nox_index_int == INT_INVALID ? NAN : nox_index_int / 10.0f;

    if (this->pm_1_0_sensor_ != nullptr)
      this->pm_1_0_sensor_->publish_state(pm_1_0);
    if (this->pm_2_5_sensor_ != nullptr)
      this->pm_2_5_sensor_->publish_state(pm_2_5);
    if (this->pm_4_0_sensor_ != nullptr)
      this->pm_4_0_sensor_->publish_state(pm_4_0);
    if (this->pm_10_0_sensor_ != nullptr)
      this->pm_10_0_sensor_->publish_state(pm_10_0);

    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(ambient_humi);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(ambient_temp);

    if (this->voc_sensor_ != nullptr)
      this->voc_sensor_->publish_state(voc_index);
    if (this->nox_sensor_ != nullptr)
      this->nox_sensor_->publish_state(nox_index);

    this->status_clear_warning();
    this->skipped_data_read_cycles_ = 0;
  });
}

bool SEN5xComponent::write_command_(uint16_t command) {
  // Warning ugly, trick the I2Ccomponent base by setting register to the first 8 bit.
  return this->write_byte(command >> 8, command & 0xFF);
}

// NB Andrew : this is the same
uint8_t SEN5xComponent::sht_crc_(uint8_t data1, uint8_t data2) {
  uint8_t bit;
  uint8_t crc = 0xFF;

  crc ^= data1;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80) {
      crc = (crc << 1) ^ 0x131;
    } else {
      crc = (crc << 1);
    }
  }

  crc ^= data2;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80) {
      crc = (crc << 1) ^ 0x131;
    } else {
      crc = (crc << 1);
    }
  }

  return crc;
}

bool SEN5xComponent::start_continuous_measurement_() {
  if (!this->write_command_(SEN5X_CMD_START_MEASUREMENT)) {
    ESP_LOGE(TAG, "Error starting measurements!");
    return false;
  }
  return true;
}

bool SEN5xComponent::read_data_(uint16_t *data, uint8_t len) {
  const uint8_t num_bytes = len * 3;
  std::vector<uint8_t> buf(num_bytes);

  if (this->read(buf.data(), num_bytes) != i2c::ERROR_OK) {
    ESP_LOGD(TAG, "I2C not ok");
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    const uint8_t j = 3 * i;
    uint8_t crc = sht_crc_(buf[j], buf[j + 1]);
    if (crc != buf[j + 2]) {
      ESP_LOGE(TAG, "CRC8 Checksum invalid at %d! 0x%02X != 0x%02X", i, buf[j + 2], crc);
      return false;
    }
    data[i] = (buf[j] << 8) | buf[j + 1];
  }

  return true;
}

}  // namespace sen5x
}  // namespace esphome

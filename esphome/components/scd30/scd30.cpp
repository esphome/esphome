#include "scd30.h"
#include "esphome/core/log.h"

namespace esphome {
namespace scd30 {

static const char *TAG = "scd30";

static const uint16_t SCD30_CMD_GET_FIRMWARE_VERSION = 0xd100;
static const uint16_t SCD30_CMD_START_CONTINUOUS_MEASUREMENTS = 0x0010;
static const uint16_t SCD30_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SCD30_CMD_READ_MEASUREMENT = 0x0300;

/// Commands for future use
static const uint16_t SCD30_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SCD30_CMD_MEASUREMENT_INTERVAL = 0x4600;
static const uint16_t SCD30_CMD_AUTOMATIC_SELF_CALIBRATION = 0x5306;
static const uint16_t SCD30_CMD_FORCED_CALIBRATION = 0x5204;
static const uint16_t SCD30_CMD_TEMPERATURE_OFFSET = 0x5403;
static const uint16_t SCD30_CMD_ALTITUDE_COMPENSATION = 0x5102;
static const uint16_t SCD30_CMD_SOFT_RESET = 0xD304;

void SCD30Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up scd30...");

  /// Firmware version identification
  if (!this->write_command_(SCD30_CMD_GET_FIRMWARE_VERSION)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  uint16_t raw_firmware_version[3];

  if (!this->read_data_(raw_firmware_version, 3)) {
    this->error_code_ = FIRMWARE_IDENTIFICATION_FAILED;
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "SCD30 Firmware v%0d.%02d", (uint16_t(raw_firmware_version[0]) >> 8),
           uint16_t(raw_firmware_version[0] & 0xFF));

  /// Sensor initialization
  if (!this->write_command_(SCD30_CMD_START_CONTINUOUS_MEASUREMENTS)) {
    ESP_LOGE(TAG, "Sensor SCD30 error starting continuous measurements.");
    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }
}

void SCD30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "scd30:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement Initialization failed!");
        break;
      case FIRMWARE_IDENTIFICATION_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void SCD30Component::update() {
  /// Check if measurement is ready before reading the value
  if (!this->write_command_(SCD30_CMD_GET_DATA_READY_STATUS)) {
    this->status_set_warning();
    return;
  }

  uint16_t raw_read_status[1];
  if (!this->read_data_(raw_read_status, 1) || raw_read_status[0] == 0x00) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Data not ready yet!");
    return;
  }

  if (!this->write_command_(SCD30_CMD_READ_MEASUREMENT)) {
    ESP_LOGW(TAG, "Error reading measurement!");
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[6];
    if (!this->read_data_(raw_data, 6)) {
      this->status_set_warning();
      return;
    }

    union uint32_float_t {
      uint32_t uint32;
      float value;
    };
    uint32_t temp_c_o2_u32 = (((uint32_t(raw_data[0])) << 16) | (uint32_t(raw_data[1])));
    uint32_float_t co2{.uint32 = temp_c_o2_u32};

    uint32_t temp_temp_u32 = (((uint32_t(raw_data[2])) << 16) | (uint32_t(raw_data[3])));
    uint32_float_t temperature{.uint32 = temp_temp_u32};

    uint32_t temp_hum_u32 = (((uint32_t(raw_data[4])) << 16) | (uint32_t(raw_data[5])));
    uint32_float_t humidity{.uint32 = temp_hum_u32};

    ESP_LOGD(TAG, "Got CO2=%.2fppm temperature=%.2f°C humidity=%.2f%%", co2.value, temperature.value, humidity.value);
    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(co2.value);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature.value);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity.value);

    this->status_clear_warning();
  });
}

bool SCD30Component::write_command_(uint16_t command) {
  // Warning ugly, trick the I2Ccomponent base by setting register to the first 8 bit.
  return this->write_byte(command >> 8, command & 0xFF);
}

uint8_t SCD30Component::sht_crc_(uint8_t data1, uint8_t data2) {
  uint8_t bit;
  uint8_t crc = 0xFF;

  crc ^= data1;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80)
      crc = (crc << 1) ^ 0x131;
    else
      crc = (crc << 1);
  }

  crc ^= data2;
  for (bit = 8; bit > 0; --bit) {
    if (crc & 0x80)
      crc = (crc << 1) ^ 0x131;
    else
      crc = (crc << 1);
  }

  return crc;
}

bool SCD30Component::read_data_(uint16_t *data, uint8_t len) {
  const uint8_t num_bytes = len * 3;
  auto *buf = new uint8_t[num_bytes];

  if (!this->parent_->raw_receive(this->address_, buf, num_bytes)) {
    delete[](buf);
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    const uint8_t j = 3 * i;
    uint8_t crc = sht_crc_(buf[j], buf[j + 1]);
    if (crc != buf[j + 2]) {
      ESP_LOGE(TAG, "CRC8 Checksum invalid! 0x%02X != 0x%02X", buf[j + 2], crc);
      delete[](buf);
      return false;
    }
    data[i] = (buf[j] << 8) | buf[j + 1];
  }

  delete[](buf);
  return true;
}

}  // namespace scd30
}  // namespace esphome

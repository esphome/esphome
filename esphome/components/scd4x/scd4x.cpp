#include "scd4x.h"
#include "esphome/core/log.h"

namespace esphome {
namespace scd4x {

static const char *TAG = "scd4x";

static const uint16_t SCD4X_CMD_GET_SERIAL_NUMBER = 0x3682;
static const uint16_t SCD4X_CMD_TEMPERATURE_OFFSET = 0x241d;
static const uint16_t SCD4X_CMD_ALTITUDE_COMPENSATION = 0x2427;
static const uint16_t SCD4X_CMD_AMBIENT_PRESSURE_COMPENSATION = 0xe000;
static const uint16_t SCD4X_CMD_AUTOMATIC_SELF_CALIBRATION = 0x2416;
static const uint16_t SCD4X_CMD_START_CONTINUOUS_MEASUREMENTS = 0x21b1;
static const uint16_t SCD4X_CMD_GET_DATA_READY_STATUS = 0xe4b8;
static const uint16_t SCD4X_CMD_READ_MEASUREMENT = 0xec05;
static const uint16_t SCD4X_CMD_PERFORM_FORCED_CALIBRATION = 0x362f;
static const uint16_t SCD4X_CMD_STOP_MEASUREMENTS = 0x3f86;

static const float SCD4X_TEMPERATURE_OFFSET_MULTIPLIER = (1 << 16) / 175.0f;

float raw_temperature_to_temperature(uint16_t raw_temperature) {
  return -45.0f + (175.0f * (raw_temperature)) / (1 << 16);
}

float raw_humidity_to_humidity(uint16_t raw_humidity) { return (100.0f * raw_humidity) / (1 << 16); }

void SCD4XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up scd4x...");

#ifdef ARDUINO_ARCH_ESP8266
  Wire.setClockStretchLimit(150000);
#endif
  this->set_timeout(1000, [this]() {
    // Check if measurement is ready before reading the value
    if (!this->write_command_(SCD4X_CMD_GET_DATA_READY_STATUS)) {
      ESP_LOGE(TAG, "Failed to write data ready status command");
      this->mark_failed();
      return;
    }

    uint16_t raw_read_status[1];
    if (!this->read_data_(raw_read_status, 1)) {
      ESP_LOGE(TAG, "Failed to read data ready status");
      this->mark_failed();
      return;
    }
    ESP_LOGD(TAG, "Data ready: %d", raw_read_status[0]);

    if (raw_read_status[0]) {
      ESP_LOGD(TAG, "Sensor already has data, resetting");
      if (!this->write_command_(SCD4X_CMD_STOP_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Failed to stop measurements");
        this->mark_failed();
        return;
      }
    }

    if (!this->write_command_(SCD4X_CMD_GET_SERIAL_NUMBER)) {
      ESP_LOGE(TAG, "Failed to write get serial command");
      this->error_code_ = COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }
    uint16_t raw_serial_number[3];

    if (!this->read_data_(raw_serial_number, 3)) {
      ESP_LOGE(TAG, "Failed to read serial number");
      this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
      this->mark_failed();
      return;
    }
    ESP_LOGD(TAG, "SCD4X serial number %02d.%02d.%02d", (uint16_t(raw_serial_number[0]) >> 8),
             uint16_t(raw_serial_number[0] & 0xFF), (uint16_t(raw_serial_number[1]) >> 8));

    if (this->temperature_offset_ != 0) {
      if (!this->write_command_(SCD4X_CMD_TEMPERATURE_OFFSET,
                                (uint16_t)(temperature_offset_ * SCD4X_TEMPERATURE_OFFSET_MULTIPLIER))) {
        ESP_LOGE(TAG, "Sensor SCD4X error setting temperature offset.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
    }

    if (this->altitude_compensation_ != 0xFFFF) {
      if (!this->write_command_(SCD4X_CMD_ALTITUDE_COMPENSATION, altitude_compensation_)) {
        ESP_LOGE(TAG, "Sensor SCD4X error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
    }

    if (this->ambient_pressure_compensation_ != 0xFFFF) {
      if (!this->write_command_(SCD4X_CMD_AMBIENT_PRESSURE_COMPENSATION, ambient_pressure_compensation_)) {
        ESP_LOGE(TAG, "Sensor SCD4X error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
    }

    if (!this->write_command_(SCD4X_CMD_AUTOMATIC_SELF_CALIBRATION, enable_asc_ ? 1 : 0)) {
      ESP_LOGE(TAG, "Sensor SCD4X error setting automatic self calibration.");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->mark_failed();
      return;
    }

    if (!this->write_command_(SCD4X_CMD_START_CONTINUOUS_MEASUREMENTS)) {
      ESP_LOGE(TAG, "Sensor SCD4X error starting continuous measurements.");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->mark_failed();
      return;
    }

    initialized_ = true;
    ESP_LOGD(TAG, "Sensor initialized");
  });
}

void SCD4XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "scd4x:");
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
        ESP_LOGW(TAG, "Unable to read sensor firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  if (this->altitude_compensation_ == 0xFFFF) {
    ESP_LOGCONFIG(TAG, "  Altitude compensation: OFF");
  } else {
    ESP_LOGCONFIG(TAG, "  Altitude compensation: %dm", this->altitude_compensation_);
  }
  ESP_LOGCONFIG(TAG, "  Automatic self calibration: %s", ONOFF(this->enable_asc_));
  ESP_LOGCONFIG(TAG, "  Ambient pressure compensation: %dmBar", this->ambient_pressure_compensation_);
  ESP_LOGCONFIG(TAG, "  Temperature offset: %.2f °C", this->temperature_offset_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void SCD4XComponent::update() {
  if (!initialized_) {
    return;
  }
  /// Check if measurement is ready before reading the value
  if (!this->write_command_(SCD4X_CMD_GET_DATA_READY_STATUS)) {
    this->status_set_warning();
    return;
  }

  uint16_t raw_read_status[1];
  if (!this->read_data_(raw_read_status, 1) || raw_read_status[0] == 0x00) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Data not ready yet!");
    return;
  }

  if (!this->write_command_(SCD4X_CMD_READ_MEASUREMENT)) {
    ESP_LOGW(TAG, "Error reading measurement!");
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    union reading_t {
      uint16_t raw_data[3];
      struct {
        uint16_t co2, raw_temperature, raw_humidity;
      };
    };
    reading_t reading;

    if (!this->read_data_(reading.raw_data, 3)) {
      this->status_set_warning();
      return;
    }

    const float temperature = raw_temperature_to_temperature(reading.raw_temperature);
    const float humidity = raw_humidity_to_humidity(reading.raw_humidity);
    ESP_LOGD(TAG, "Got CO2=%dppm temperature=%.2f°C humidity=%.2f%%", reading.co2, temperature, humidity);
    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(reading.co2);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);

    this->status_clear_warning();
  });
}

void SCD4XComponent::set_forced_recalibration_value(uint16_t value) {
  if (!initialized_) {
    return;
  }
  if (!this->write_command_(SCD4X_CMD_STOP_MEASUREMENTS)) {
    ESP_LOGW(TAG, "Error stopping measurements");
    this->status_set_warning();
    return;
  }
  this->set_timeout(500, [this, value]() {
    if (!this->write_command_(SCD4X_CMD_PERFORM_FORCED_CALIBRATION, value)) {
      ESP_LOGW(TAG, "Error calibrating!");
      this->status_set_warning();
    }
    this->set_timeout(500, [this]() {
      uint16_t correction[1];

      if (!this->read_data_(correction, 1)) {
        ESP_LOGW(TAG, "Failed to read correction data");
      } else {
        ESP_LOGD(TAG, "Correction %d", ((correction[0] >> 8) - 0x8000));
      }
      if (!this->write_command_(SCD4X_CMD_START_CONTINUOUS_MEASUREMENTS)) {
        ESP_LOGW(TAG, "Error re-starting measurements after forced calibration");
      }
    });
  });
}

bool SCD4XComponent::write_command_(uint16_t command) {
  // Warning ugly, trick the I2Ccomponent base by setting register to the first 8 bit.
  return this->write_byte(command >> 8, command & 0xFF);
}

bool SCD4XComponent::write_command_(uint16_t command, uint16_t data) {
  uint8_t raw[5];
  raw[0] = command >> 8;
  raw[1] = command & 0xFF;
  raw[2] = data >> 8;
  raw[3] = data & 0xFF;
  raw[4] = sht_crc_(raw[2], raw[3]);
  return this->write_bytes_raw(raw, 5);
}

uint8_t SCD4XComponent::sht_crc_(uint8_t data1, uint8_t data2) {
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

bool SCD4XComponent::read_data_(uint16_t *data, uint8_t len) {
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

}  // namespace scd4x
}  // namespace esphome

#include "sgp30.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sgp30 {

static const char *TAG = "sgp30";

static const uint16_t SGP30_CMD_GET_SERIAL_ID = 0x3682;
static const uint16_t SGP30_CMD_GET_FEATURESET = 0x202f;
static const uint16_t SGP30_CMD_IAQ_INIT = 0x2003;
static const uint16_t SGP30_CMD_MEASURE_IAQ = 0x2008;
static const uint16_t SGP30_CMD_SET_ABSOLUTE_HUMIDITY = 0x2061;
static const uint16_t SGP30_CMD_GET_IAQ_BASELINE = 0x2015;
static const uint16_t SGP30_CMD_SET_IAQ_BASELINE = 0x201E;

// Sensor baseline should first be relied on after 1H of operation,
// if the sensor starts with a baseline value provided
const long IAQ_BASELINE_WARM_UP_SECONDS_WITH_BASELINE_PROVIDED = 3600;

// Sensor baseline could first be relied on after 12H of operation,
// if the sensor starts without any prior baseline value provided
const long IAQ_BASELINE_WARM_UP_SECONDS_WITHOUT_BASELINE = 43200;

void SGP30Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SGP30...");

  // Serial Number identification
  if (!this->write_command_(SGP30_CMD_GET_SERIAL_ID)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  uint16_t raw_serial_number[3];

  if (!this->read_data_(raw_serial_number, 3)) {
    this->mark_failed();
    return;
  }
  this->serial_number_ = (uint64_t(raw_serial_number[0]) << 24) | (uint64_t(raw_serial_number[1]) << 16) |
                         (uint64_t(raw_serial_number[2]));
  ESP_LOGD(TAG, "Serial Number: %llu", this->serial_number_);

  // Featureset identification for future use
  if (!this->write_command_(SGP30_CMD_GET_FEATURESET)) {
    this->mark_failed();
    return;
  }
  uint16_t raw_featureset[1];
  if (!this->read_data_(raw_featureset, 1)) {
    this->mark_failed();
    return;
  }
  this->featureset_ = raw_featureset[0];
  if (uint16_t(this->featureset_ >> 12) != 0x0) {
    if (uint16_t(this->featureset_ >> 12) == 0x1) {
      // ID matching a different sensor: SGPC3
      this->error_code_ = UNSUPPORTED_ID;
    } else {
      // Unknown ID
      this->error_code_ = INVALID_ID;
    }
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Product version: 0x%0X", uint16_t(this->featureset_ & 0x1FF));

  // Sensor initialization
  if (!this->write_command_(SGP30_CMD_IAQ_INIT)) {
    ESP_LOGE(TAG, "Sensor sgp30_iaq_init failed.");
    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }

  // Sensor baseline reliability timer
  if (this->baseline_ > 0) {
    this->required_warm_up_time_ = IAQ_BASELINE_WARM_UP_SECONDS_WITH_BASELINE_PROVIDED;
    this->write_iaq_baseline_(this->baseline_);
  } else {
    this->required_warm_up_time_ = IAQ_BASELINE_WARM_UP_SECONDS_WITHOUT_BASELINE;
  }
}

bool SGP30Component::is_sensor_baseline_reliable_() {
  if ((this->required_warm_up_time_ == 0) || (std::floor(millis() / 1000) >= this->required_warm_up_time_)) {
    // requirement for warm up is removed once the millis uptime surpasses the required warm_up_time
    // this avoids the repetitive warm up when the millis uptime is rolled over every ~40 days
    this->required_warm_up_time_ = 0;
    return true;
  }
  return false;
}

void SGP30Component::read_iaq_baseline_() {
  if (this->is_sensor_baseline_reliable_()) {
    if (!this->write_command_(SGP30_CMD_GET_IAQ_BASELINE)) {
      ESP_LOGD(TAG, "Error getting baseline");
      this->status_set_warning();
      return;
    }
    this->set_timeout(50, [this]() {
      uint16_t raw_data[2];
      if (!this->read_data_(raw_data, 2)) {
        this->status_set_warning();
        return;
      }

      uint8_t eco2baseline = (raw_data[0]);
      uint8_t tvocbaseline = (raw_data[1]);

      ESP_LOGI(TAG, "Current eCO2 & TVOC baseline: 0x%04X", uint16_t((eco2baseline << 8) | (tvocbaseline & 0xFF)));
      this->status_clear_warning();
    });
  } else {
    ESP_LOGD(TAG, "Baseline reading not available for: %.0fs",
             (this->required_warm_up_time_ - std::floor(millis() / 1000)));
  }
}

void SGP30Component::send_env_data_() {
  if (this->humidity_sensor_ == nullptr && this->temperature_sensor_ == nullptr)
    return;
  float humidity = NAN;
  if (this->humidity_sensor_ != nullptr)
    humidity = this->humidity_sensor_->state;
  if (isnan(humidity) || humidity < 0.0f || humidity > 100.0f) {
    ESP_LOGW(TAG, "Compensation not possible yet: bad humidity data.");
    return;
  } else {
    ESP_LOGD(TAG, "External compensation data received: Humidity %0.2f%%", humidity);
  }
  float temperature = NAN;
  if (this->temperature_sensor_ != nullptr) {
    temperature = float(this->temperature_sensor_->state);
  }
  if (isnan(temperature) || temperature < -40.0f || temperature > 85.0f) {
    ESP_LOGW(TAG, "Compensation not possible yet: bad temperature value data.");
    return;
  } else {
    ESP_LOGD(TAG, "External compensation data received: Temperature %0.2f°C", temperature);
  }

  float absolute_humidity =
      216.7f * (((humidity / 100) * 6.112f * std::exp((17.62f * temperature) / (243.12f + temperature))) /
                (273.15f + temperature));
  uint8_t humidity_full = uint8_t(std::floor(absolute_humidity));
  uint8_t humidity_dec = uint8_t(std::floor((absolute_humidity - std::floor(absolute_humidity)) * 256));
  ESP_LOGD(TAG, "Calculated Absolute humidity: %0.3f g/m³ (0x%04X)", absolute_humidity,
           uint16_t(uint16_t(humidity_full) << 8 | uint16_t(humidity_dec)));
  uint8_t crc = sht_crc_(humidity_full, humidity_dec);
  uint8_t data[4];
  data[0] = SGP30_CMD_SET_ABSOLUTE_HUMIDITY & 0xFF;
  data[1] = humidity_full;
  data[2] = humidity_dec;
  data[3] = crc;
  if (!this->write_bytes(SGP30_CMD_SET_ABSOLUTE_HUMIDITY >> 8, data, 4)) {
    ESP_LOGE(TAG, "Error sending compensation data.");
  }
}

void SGP30Component::write_iaq_baseline_(uint16_t baseline) {
  uint8_t e_c_o2_baseline = baseline >> 8;
  uint8_t tvoc_baseline = baseline & 0xFF;
  uint8_t data[4];
  data[0] = SGP30_CMD_SET_IAQ_BASELINE & 0xFF;
  data[1] = e_c_o2_baseline;
  data[2] = tvoc_baseline;
  data[3] = sht_crc_(e_c_o2_baseline, tvoc_baseline);
  if (!this->write_bytes(SGP30_CMD_SET_IAQ_BASELINE >> 8, data, 4)) {
    ESP_LOGE(TAG, "Error applying baseline: 0x%04X", baseline);
  } else
    ESP_LOGI(TAG, "Initial baseline 0x%04X applied successfully!", baseline);
}

void SGP30Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SGP30:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement Initialization failed!");
        break;
      case INVALID_ID:
        ESP_LOGW(TAG, "Sensor reported an invalid ID. Is this an SGP30?");
        break;
      case UNSUPPORTED_ID:
        ESP_LOGW(TAG, "Sensor reported an unsupported ID (SGPC3).");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  } else {
    ESP_LOGCONFIG(TAG, "  Serial number: %llu", this->serial_number_);
    ESP_LOGCONFIG(TAG, "  Baseline: 0x%04X%s", this->baseline_,
                  ((this->baseline_ != 0x0000) ? " (enabled)" : " (disabled)"));
    ESP_LOGCONFIG(TAG, "  Warm up time: %lds", this->required_warm_up_time_);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "eCO2", this->eco2_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
  if (this->humidity_sensor_ != nullptr && this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Compensation:");
    LOG_SENSOR("    ", "Temperature Source:", this->temperature_sensor_);
    LOG_SENSOR("    ", "Humidity Source:", this->humidity_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Compensation: No source configured");
  }
}

void SGP30Component::update() {
  if (!this->write_command_(SGP30_CMD_MEASURE_IAQ)) {
    this->status_set_warning();
    return;
  }

  this->set_timeout(50, [this]() {
    uint16_t raw_data[2];
    if (!this->read_data_(raw_data, 2)) {
      this->status_set_warning();
      return;
    }

    float eco2 = (raw_data[0]);
    float tvoc = (raw_data[1]);

    ESP_LOGD(TAG, "Got eCO2=%.1fppm TVOC=%.1fppb", eco2, tvoc);
    if (this->eco2_sensor_ != nullptr)
      this->eco2_sensor_->publish_state(eco2);
    if (this->tvoc_sensor_ != nullptr)
      this->tvoc_sensor_->publish_state(tvoc);
    this->status_clear_warning();
    this->send_env_data_();
    this->read_iaq_baseline_();
  });
}

bool SGP30Component::write_command_(uint16_t command) {
  // Warning ugly, trick the I2Ccomponent base by setting register to the first 8 bit.
  return this->write_byte(command >> 8, command & 0xFF);
}

uint8_t SGP30Component::sht_crc_(uint8_t data1, uint8_t data2) {
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

bool SGP30Component::read_data_(uint16_t *data, uint8_t len) {
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

}  // namespace sgp30
}  // namespace esphome

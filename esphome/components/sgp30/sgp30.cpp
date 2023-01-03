#include "sgp30.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cinttypes>

namespace esphome {
namespace sgp30 {

static const char *const TAG = "sgp30";

static const uint16_t SGP30_CMD_GET_SERIAL_ID = 0x3682;
static const uint16_t SGP30_CMD_GET_FEATURESET = 0x202f;
static const uint16_t SGP30_CMD_IAQ_INIT = 0x2003;
static const uint16_t SGP30_CMD_MEASURE_IAQ = 0x2008;
static const uint16_t SGP30_CMD_SET_ABSOLUTE_HUMIDITY = 0x2061;
static const uint16_t SGP30_CMD_GET_IAQ_BASELINE = 0x2015;
static const uint16_t SGP30_CMD_SET_IAQ_BASELINE = 0x201E;

// Sensor baseline should first be relied on after 1H of operation,
// if the sensor starts with a baseline value provided
const uint32_t IAQ_BASELINE_WARM_UP_SECONDS_WITH_BASELINE_PROVIDED = 3600;

// Sensor baseline could first be relied on after 12H of operation,
// if the sensor starts without any prior baseline value provided
const uint32_t IAQ_BASELINE_WARM_UP_SECONDS_WITHOUT_BASELINE = 43200;

// Shortest time interval of 1H for storing baseline values.
// Prevents wear of the flash because of too many write operations
const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 3600;

// Store anyway if the baseline difference exceeds the max storage diff value
const uint32_t MAXIMUM_STORAGE_DIFF = 50;

void SGP30Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SGP30...");

  // Serial Number identification
  uint16_t raw_serial_number[3];
  if (!this->get_register(SGP30_CMD_GET_SERIAL_ID, raw_serial_number, 3)) {
    this->mark_failed();
    return;
  }
  this->serial_number_ = (uint64_t(raw_serial_number[0]) << 24) | (uint64_t(raw_serial_number[1]) << 16) |
                         (uint64_t(raw_serial_number[2]));
  ESP_LOGD(TAG, "Serial Number: %" PRIu64, this->serial_number_);

  // Featureset identification for future use
  uint16_t raw_featureset;
  if (!this->get_register(SGP30_CMD_GET_FEATURESET, raw_featureset)) {
    this->mark_failed();
    return;
  }
  this->featureset_ = raw_featureset;
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
  if (!this->write_command(SGP30_CMD_IAQ_INIT)) {
    ESP_LOGE(TAG, "Sensor sgp30_iaq_init failed.");
    this->error_code_ = MEASUREMENT_INIT_FAILED;
    this->mark_failed();
    return;
  }

  // Hash with compilation time
  // This ensures the baseline storage is cleared after OTA
  uint32_t hash = fnv1_hash(App.get_compilation_time());
  this->pref_ = global_preferences->make_preference<SGP30Baselines>(hash, true);

  if (this->pref_.load(&this->baselines_storage_)) {
    ESP_LOGI(TAG, "Loaded eCO2 baseline: 0x%04X, TVOC baseline: 0x%04X", this->baselines_storage_.eco2,
             baselines_storage_.tvoc);
    this->eco2_baseline_ = this->baselines_storage_.eco2;
    this->tvoc_baseline_ = this->baselines_storage_.tvoc;
  }

  // Initialize storage timestamp
  this->seconds_since_last_store_ = 0;

  // Sensor baseline reliability timer
  if (this->eco2_baseline_ > 0 && this->tvoc_baseline_ > 0) {
    this->required_warm_up_time_ = IAQ_BASELINE_WARM_UP_SECONDS_WITH_BASELINE_PROVIDED;
    this->write_iaq_baseline_(this->eco2_baseline_, this->tvoc_baseline_);
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
    if (!this->write_command(SGP30_CMD_GET_IAQ_BASELINE)) {
      ESP_LOGD(TAG, "Error getting baseline");
      this->status_set_warning();
      return;
    }
    this->set_timeout(50, [this]() {
      uint16_t raw_data[2];
      if (!this->read_data(raw_data, 2)) {
        this->status_set_warning();
        return;
      }

      uint16_t eco2baseline = (raw_data[0]);
      uint16_t tvocbaseline = (raw_data[1]);

      ESP_LOGI(TAG, "Current eCO2 baseline: 0x%04X, TVOC baseline: 0x%04X", eco2baseline, tvocbaseline);
      if (eco2baseline != this->eco2_baseline_ || tvocbaseline != this->tvoc_baseline_) {
        this->eco2_baseline_ = eco2baseline;
        this->tvoc_baseline_ = tvocbaseline;
        if (this->eco2_sensor_baseline_ != nullptr)
          this->eco2_sensor_baseline_->publish_state(this->eco2_baseline_);
        if (this->tvoc_sensor_baseline_ != nullptr)
          this->tvoc_sensor_baseline_->publish_state(this->tvoc_baseline_);

        // Store baselines after defined interval or if the difference between current and stored baseline becomes too
        // much
        if (this->store_baseline_ &&
            (this->seconds_since_last_store_ > SHORTEST_BASELINE_STORE_INTERVAL ||
             (uint32_t) abs(this->baselines_storage_.eco2 - this->eco2_baseline_) > MAXIMUM_STORAGE_DIFF ||
             (uint32_t) abs(this->baselines_storage_.tvoc - this->tvoc_baseline_) > MAXIMUM_STORAGE_DIFF)) {
          this->seconds_since_last_store_ = 0;
          this->baselines_storage_.eco2 = this->eco2_baseline_;
          this->baselines_storage_.tvoc = this->tvoc_baseline_;
          if (this->pref_.save(&this->baselines_storage_)) {
            ESP_LOGI(TAG, "Store eCO2 baseline: 0x%04X, TVOC baseline: 0x%04X", this->baselines_storage_.eco2,
                     this->baselines_storage_.tvoc);
          } else {
            ESP_LOGW(TAG, "Could not store eCO2 and TVOC baselines");
          }
        }
      }
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
  if (std::isnan(humidity) || humidity < 0.0f || humidity > 100.0f) {
    ESP_LOGW(TAG, "Compensation not possible yet: bad humidity data.");
    return;
  } else {
    ESP_LOGD(TAG, "External compensation data received: Humidity %0.2f%%", humidity);
  }
  float temperature = NAN;
  if (this->temperature_sensor_ != nullptr) {
    temperature = float(this->temperature_sensor_->state);
  }
  if (std::isnan(temperature) || temperature < -40.0f || temperature > 85.0f) {
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

void SGP30Component::write_iaq_baseline_(uint16_t eco2_baseline, uint16_t tvoc_baseline) {
  uint8_t data[7];
  data[0] = SGP30_CMD_SET_IAQ_BASELINE & 0xFF;
  data[1] = tvoc_baseline >> 8;
  data[2] = tvoc_baseline & 0xFF;
  data[3] = sht_crc_(data[1], data[2]);
  data[4] = eco2_baseline >> 8;
  data[5] = eco2_baseline & 0xFF;
  data[6] = sht_crc_(data[4], data[5]);
  if (!this->write_bytes(SGP30_CMD_SET_IAQ_BASELINE >> 8, data, 7)) {
    ESP_LOGE(TAG, "Error applying eCO2 baseline: 0x%04X, TVOC baseline: 0x%04X", eco2_baseline, tvoc_baseline);
  } else {
    ESP_LOGI(TAG, "Initial baselines applied successfully! eCO2 baseline: 0x%04X, TVOC baseline: 0x%04X", eco2_baseline,
             tvoc_baseline);
  }
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
    ESP_LOGCONFIG(TAG, "  Serial number: %" PRIu64, this->serial_number_);
    if (this->eco2_baseline_ != 0x0000 && this->tvoc_baseline_ != 0x0000) {
      ESP_LOGCONFIG(TAG, "  Baseline:");
      ESP_LOGCONFIG(TAG, "    eCO2 Baseline: 0x%04X", this->eco2_baseline_);
      ESP_LOGCONFIG(TAG, "    TVOC Baseline: 0x%04X", this->tvoc_baseline_);
    } else {
      ESP_LOGCONFIG(TAG, "  Baseline: No baseline configured");
    }
    ESP_LOGCONFIG(TAG, "  Warm up time: %us", this->required_warm_up_time_);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "eCO2 sensor", this->eco2_sensor_);
  LOG_SENSOR("  ", "TVOC sensor", this->tvoc_sensor_);
  LOG_SENSOR("  ", "eCO2 baseline sensor", this->eco2_sensor_baseline_);
  LOG_SENSOR("  ", "TVOC baseline sensor", this->tvoc_sensor_baseline_);
  ESP_LOGCONFIG(TAG, "Store baseline: %s", YESNO(this->store_baseline_));
  if (this->humidity_sensor_ != nullptr && this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Compensation:");
    LOG_SENSOR("    ", "Temperature Source:", this->temperature_sensor_);
    LOG_SENSOR("    ", "Humidity Source:", this->humidity_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Compensation: No source configured");
  }
}

void SGP30Component::update() {
  if (!this->write_command(SGP30_CMD_MEASURE_IAQ)) {
    this->status_set_warning();
    return;
  }
  this->seconds_since_last_store_ += this->update_interval_ / 1000;
  this->set_timeout(50, [this]() {
    uint16_t raw_data[2];
    if (!this->read_data(raw_data, 2)) {
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

    if (this->get_update_interval() != 1000) {
      ESP_LOGW(TAG, "Update interval for SGP30 sensor must be set to 1s for optimized readout");
    }

    this->status_clear_warning();
    this->send_env_data_();
    this->read_iaq_baseline_();
  });
}

}  // namespace sgp30
}  // namespace esphome

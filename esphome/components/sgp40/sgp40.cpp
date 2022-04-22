#include "sgp40.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace sgp40 {

static const char *const TAG = "sgp40";

void SGP40Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SGP40...");

  // Serial Number identification
  if (!this->write_command(SGP40_CMD_GET_SERIAL_ID)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  uint16_t raw_serial_number[3];

  if (!this->read_data(raw_serial_number, 3)) {
    this->mark_failed();
    return;
  }
  this->serial_number_ = (uint64_t(raw_serial_number[0]) << 24) | (uint64_t(raw_serial_number[1]) << 16) |
                         (uint64_t(raw_serial_number[2]));
  ESP_LOGD(TAG, "Serial Number: %" PRIu64, this->serial_number_);

  // Featureset identification for future use
  if (!this->write_command(SGP40_CMD_GET_FEATURESET)) {
    ESP_LOGD(TAG, "raw_featureset write_command_ failed");
    this->mark_failed();
    return;
  }
  uint16_t raw_featureset;
  if (!this->read_data(raw_featureset)) {
    ESP_LOGD(TAG, "raw_featureset read_data_ failed");
    this->mark_failed();
    return;
  }

  this->featureset_ = raw_featureset;
  if ((this->featureset_ & 0x1FF) != SGP40_FEATURESET) {
    ESP_LOGD(TAG, "Product feature set failed 0x%0X , expecting 0x%0X", uint16_t(this->featureset_ & 0x1FF),
             SGP40_FEATURESET);
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "Product version: 0x%0X", uint16_t(this->featureset_ & 0x1FF));

  voc_algorithm_init(&this->voc_algorithm_params_);

  if (this->store_baseline_) {
    // Hash with compilation time
    // This ensures the baseline storage is cleared after OTA
    uint32_t hash = fnv1_hash(App.get_compilation_time());
    this->pref_ = global_preferences->make_preference<SGP40Baselines>(hash, true);

    if (this->pref_.load(&this->baselines_storage_)) {
      this->state0_ = this->baselines_storage_.state0;
      this->state1_ = this->baselines_storage_.state1;
      ESP_LOGI(TAG, "Loaded VOC baseline state0: 0x%04X, state1: 0x%04X", this->baselines_storage_.state0,
               baselines_storage_.state1);
    }

    // Initialize storage timestamp
    this->seconds_since_last_store_ = 0;

    if (this->baselines_storage_.state0 > 0 && this->baselines_storage_.state1 > 0) {
      ESP_LOGI(TAG, "Setting VOC baseline from save state0: 0x%04X, state1: 0x%04X", this->baselines_storage_.state0,
               baselines_storage_.state1);
      voc_algorithm_set_states(&this->voc_algorithm_params_, this->baselines_storage_.state0,
                               this->baselines_storage_.state1);
    }
  }

  this->self_test_();

  /* The official spec for this sensor at https://docs.rs-online.com/1956/A700000007055193.pdf
  indicates this sensor should be driven at 1Hz. Comments from the developers at:
  https://github.com/Sensirion/embedded-sgp/issues/136 indicate the algorithm should be a bit
  resilient to slight timing variations so the software timer should be accurate enough for
  this.

  This block starts sampling from the sensor at 1Hz, and is done seperately from the call
  to the update method. This seperation is to support getting accurate measurements but
  limit the amount of communication done over wifi for power consumption or to keep the
  number of records reported from being overwhelming.
  */
  ESP_LOGD(TAG, "Component requires sampling of 1Hz, setting up background sampler");
  this->set_interval(1000, [this]() { this->update_voc_index(); });
}

void SGP40Component::self_test_() {
  ESP_LOGD(TAG, "Self-test started");
  if (!this->write_command(SGP40_CMD_SELF_TEST)) {
    this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGD(TAG, "Self-test communication failed");
    this->mark_failed();
  }

  this->set_timeout(250, [this]() {
    uint16_t reply;
    if (!this->read_data(reply)) {
      ESP_LOGD(TAG, "Self-test read_data_ failed");
      this->mark_failed();
      return;
    }

    if (reply == 0xD400) {
      this->self_test_complete_ = true;
      ESP_LOGD(TAG, "Self-test completed");
      return;
    }

    ESP_LOGD(TAG, "Self-test failed");
    this->mark_failed();
  });
}

/**
 * @brief Combined the measured gasses, temperature, and humidity
 * to calculate the VOC Index
 *
 * @param temperature The measured temperature in degrees C
 * @param humidity The measured relative humidity in % rH
 * @return int32_t The VOC Index
 */
int32_t SGP40Component::measure_voc_index_() {
  int32_t voc_index;

  uint16_t sraw = measure_raw_();

  if (sraw == UINT16_MAX)
    return UINT16_MAX;

  this->status_clear_warning();

  voc_algorithm_process(&voc_algorithm_params_, sraw, &voc_index);

  // Store baselines after defined interval or if the difference between current and stored baseline becomes too
  // much
  if (this->store_baseline_ && this->seconds_since_last_store_ > SHORTEST_BASELINE_STORE_INTERVAL) {
    voc_algorithm_get_states(&voc_algorithm_params_, &this->state0_, &this->state1_);
    if ((uint32_t) abs(this->baselines_storage_.state0 - this->state0_) > MAXIMUM_STORAGE_DIFF ||
        (uint32_t) abs(this->baselines_storage_.state1 - this->state1_) > MAXIMUM_STORAGE_DIFF) {
      this->seconds_since_last_store_ = 0;
      this->baselines_storage_.state0 = this->state0_;
      this->baselines_storage_.state1 = this->state1_;

      if (this->pref_.save(&this->baselines_storage_)) {
        ESP_LOGI(TAG, "Stored VOC baseline state0: 0x%04X ,state1: 0x%04X", this->baselines_storage_.state0,
                 baselines_storage_.state1);
      } else {
        ESP_LOGW(TAG, "Could not store VOC baselines");
      }
    }
  }

  return voc_index;
}

/**
 * @brief Return the raw gas measurement
 *
 * @param temperature The measured temperature in degrees C
 * @param humidity The measured relative humidity in % rH
 * @return uint16_t The current raw gas measurement
 */
uint16_t SGP40Component::measure_raw_() {
  float humidity = NAN;

  if (!this->self_test_complete_) {
    ESP_LOGD(TAG, "Self-test not yet complete");
    return UINT16_MAX;
  }

  if (this->humidity_sensor_ != nullptr) {
    humidity = this->humidity_sensor_->state;
  }
  if (std::isnan(humidity) || humidity < 0.0f || humidity > 100.0f) {
    humidity = 50;
  }

  float temperature = NAN;
  if (this->temperature_sensor_ != nullptr) {
    temperature = float(this->temperature_sensor_->state);
  }
  if (std::isnan(temperature) || temperature < -40.0f || temperature > 85.0f) {
    temperature = 25;
  }

  uint16_t data[2];
  uint16_t rhticks = llround((uint16_t)((humidity * 65535) / 100));
  uint16_t tempticks = (uint16_t)(((temperature + 45) * 65535) / 175);
  // first paramater is the relative humidity ticks
  data[0] = rhticks;
  // second paramater is the temperature ticks
  data[1] = tempticks;

  if (!this->write_command(SGP40_CMD_MEASURE_RAW, data, 2)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "write error (%d)", this->last_error_);
    return false;
  }
  delay(30);

  uint16_t raw_data;
  if (!this->read_data(raw_data)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "read_data_ error");
    return UINT16_MAX;
  }
  return raw_data;
}

void SGP40Component::update_voc_index() {
  this->seconds_since_last_store_ += 1;

  this->voc_index_ = this->measure_voc_index_();
  if (this->samples_read_ < this->samples_to_stabalize_) {
    this->samples_read_++;
    ESP_LOGD(TAG, "Sensor has not collected enough samples yet. (%d/%d) VOC index is: %u", this->samples_read_,
             this->samples_to_stabalize_, this->voc_index_);
    return;
  }
}

void SGP40Component::update() {
  if (this->samples_read_ < this->samples_to_stabalize_) {
    return;
  }

  if (this->voc_index_ != UINT16_MAX) {
    this->status_clear_warning();
    this->publish_state(this->voc_index_);
  } else {
    this->status_set_warning();
  }
}

void SGP40Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SGP40:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  store_baseline: %d", this->store_baseline_);

  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  } else {
    ESP_LOGCONFIG(TAG, "  Serial number: %" PRIu64, this->serial_number_);
    ESP_LOGCONFIG(TAG, "  Minimum Samples: %f", VOC_ALGORITHM_INITIAL_BLACKOUT);
  }
  LOG_UPDATE_INTERVAL(this);

  if (this->humidity_sensor_ != nullptr && this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Compensation:");
    LOG_SENSOR("    ", "Temperature Source:", this->temperature_sensor_);
    LOG_SENSOR("    ", "Humidity Source:", this->humidity_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Compensation: No source configured");
  }
}

}  // namespace sgp40
}  // namespace esphome

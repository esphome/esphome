#include "sgp4x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace sgp4x {

static const char *const TAG = "sgp4x";

void SGP4xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SGP4x...");

  // Serial Number identification
  uint16_t raw_serial_number[3];
  if (!this->get_register(SGP4X_CMD_GET_SERIAL_ID, raw_serial_number, 3, 1)) {
    ESP_LOGE(TAG, "Failed to read serial number");
    this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
    this->mark_failed();
    return;
  }
  this->serial_number_ = (uint64_t(raw_serial_number[0]) << 24) | (uint64_t(raw_serial_number[1]) << 16) |
                         (uint64_t(raw_serial_number[2]));
  ESP_LOGD(TAG, "Serial Number: %" PRIu64, this->serial_number_);

  // Featureset identification for future use
  uint16_t raw_featureset;
  if (!this->get_register(SGP4X_CMD_GET_FEATURESET, raw_featureset, 1)) {
    ESP_LOGD(TAG, "raw_featureset write_command_ failed");
    this->mark_failed();
    return;
  }
  this->featureset_ = raw_featureset;
  if ((this->featureset_ & 0x1FF) == SGP40_FEATURESET) {
    sgp_type_ = SGP40;
    self_test_time_ = SPG40_SELFTEST_TIME;
    measure_time_ = SGP40_MEASURE_TIME;
    if (this->nox_sensor_) {
      ESP_LOGE(TAG, "Measuring NOx requires a SGP41 sensor but a SGP40 sensor is detected");
      // disable the sensor
      this->nox_sensor_->set_disabled_by_default(true);
      // make sure it's not visible in HA
      this->nox_sensor_->set_internal(true);
      this->nox_sensor_->state = NAN;
      // remove pointer to sensor
      this->nox_sensor_ = nullptr;
    }
  } else {
    if ((this->featureset_ & 0x1FF) == SGP41_FEATURESET) {
      sgp_type_ = SGP41;
      self_test_time_ = SPG41_SELFTEST_TIME;
      measure_time_ = SGP41_MEASURE_TIME;
    } else {
      ESP_LOGD(TAG, "Product feature set failed 0x%0X , expecting 0x%0X", uint16_t(this->featureset_ & 0x1FF),
               SGP40_FEATURESET);
      this->mark_failed();
      return;
    }
  }

  ESP_LOGD(TAG, "Product version: 0x%0X", uint16_t(this->featureset_ & 0x1FF));

  if (this->store_baseline_) {
    // Hash with compilation time and serial number
    // This ensures the baseline storage is cleared after OTA
    // Serial numbers are unique to each sensor, so mulitple sensors can be used without conflict
    uint32_t hash = fnv1_hash(App.get_compilation_time() + std::to_string(this->serial_number_));
    this->pref_ = global_preferences->make_preference<SGP4xBaselines>(hash, true);

    if (this->pref_.load(&this->voc_baselines_storage_)) {
      this->voc_state0_ = this->voc_baselines_storage_.state0;
      this->voc_state1_ = this->voc_baselines_storage_.state1;
      ESP_LOGI(TAG, "Loaded VOC baseline state0: 0x%04" PRIX32 ", state1: 0x%04" PRIX32,
               this->voc_baselines_storage_.state0, voc_baselines_storage_.state1);
    }

    // Initialize storage timestamp
    this->seconds_since_last_store_ = 0;

    if (this->voc_baselines_storage_.state0 > 0 && this->voc_baselines_storage_.state1 > 0) {
      ESP_LOGI(TAG, "Setting VOC baseline from save state0: 0x%04" PRIX32 ", state1: 0x%04" PRIX32,
               this->voc_baselines_storage_.state0, voc_baselines_storage_.state1);
      voc_algorithm_.set_states(this->voc_baselines_storage_.state0, this->voc_baselines_storage_.state1);
    }
  }
  if (this->voc_sensor_ && this->voc_tuning_params_.has_value()) {
    voc_algorithm_.set_tuning_parameters(
        voc_tuning_params_.value().index_offset, voc_tuning_params_.value().learning_time_offset_hours,
        voc_tuning_params_.value().learning_time_gain_hours, voc_tuning_params_.value().gating_max_duration_minutes,
        voc_tuning_params_.value().std_initial, voc_tuning_params_.value().gain_factor);
  }

  if (this->nox_sensor_ && this->nox_tuning_params_.has_value()) {
    nox_algorithm_.set_tuning_parameters(
        nox_tuning_params_.value().index_offset, nox_tuning_params_.value().learning_time_offset_hours,
        nox_tuning_params_.value().learning_time_gain_hours, nox_tuning_params_.value().gating_max_duration_minutes,
        nox_tuning_params_.value().std_initial, nox_tuning_params_.value().gain_factor);
  }

  this->self_test_();

  /* The official spec for this sensor at
  https://sensirion.com/media/documents/296373BB/6203C5DF/Sensirion_Gas_Sensors_Datasheet_SGP40.pdf indicates this
  sensor should be driven at 1Hz. Comments from the developers at:
  https://github.com/Sensirion/embedded-sgp/issues/136 indicate the algorithm should be a bit resilient to slight
  timing variations so the software timer should be accurate enough for this.

  This block starts sampling from the sensor at 1Hz, and is done separately from the call
  to the update method. This separation is to support getting accurate measurements but
  limit the amount of communication done over wifi for power consumption or to keep the
  number of records reported from being overwhelming.
  */
  ESP_LOGD(TAG, "Component requires sampling of 1Hz, setting up background sampler");
  this->set_interval(1000, [this]() { this->take_sample(); });
}

void SGP4xComponent::self_test_() {
  ESP_LOGD(TAG, "Self-test started");
  if (!this->write_command(SGP4X_CMD_SELF_TEST)) {
    this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGD(TAG, "Self-test communication failed");
    this->mark_failed();
  }

  this->set_timeout(self_test_time_, [this]() {
    uint16_t reply;
    if (!this->read_data(reply)) {
      this->error_code_ = SELF_TEST_FAILED;
      ESP_LOGD(TAG, "Self-test read_data_ failed");
      this->mark_failed();
      return;
    }

    if (reply == 0xD400) {
      this->self_test_complete_ = true;
      ESP_LOGD(TAG, "Self-test completed");
      return;
    } else {
      this->error_code_ = SELF_TEST_FAILED;
      ESP_LOGD(TAG, "Self-test failed 0x%X", reply);
      return;
    }

    ESP_LOGD(TAG, "Self-test failed 0x%X", reply);
    this->mark_failed();
  });
}

void SGP4xComponent::update_gas_indices_() {
  this->voc_index_ = this->voc_algorithm_.process(this->voc_sraw_);
  if (this->nox_sensor_ != nullptr)
    this->nox_index_ = this->nox_algorithm_.process(this->nox_sraw_);
  ESP_LOGV(TAG, "VOC = %" PRId32 ", NOx = %" PRId32, this->voc_index_, this->nox_index_);
  // Store baselines after defined interval or if the difference between current and stored baseline becomes too
  // much
  if (this->store_baseline_ && this->seconds_since_last_store_ > SHORTEST_BASELINE_STORE_INTERVAL) {
    this->voc_algorithm_.get_states(this->voc_state0_, this->voc_state1_);
    if (std::abs(this->voc_baselines_storage_.state0 - this->voc_state0_) > MAXIMUM_STORAGE_DIFF ||
        std::abs(this->voc_baselines_storage_.state1 - this->voc_state1_) > MAXIMUM_STORAGE_DIFF) {
      this->seconds_since_last_store_ = 0;
      this->voc_baselines_storage_.state0 = this->voc_state0_;
      this->voc_baselines_storage_.state1 = this->voc_state1_;

      if (this->pref_.save(&this->voc_baselines_storage_)) {
        ESP_LOGI(TAG, "Stored VOC baseline state0: 0x%04" PRIX32 " ,state1: 0x%04" PRIX32,
                 this->voc_baselines_storage_.state0, this->voc_baselines_storage_.state1);
      } else {
        ESP_LOGW(TAG, "Could not store VOC baselines");
      }
    }
  }

  if (this->samples_read_ < this->samples_to_stabilize_) {
    this->samples_read_++;
    ESP_LOGD(TAG, "Sensor has not collected enough samples yet. (%d/%d) VOC index is: %" PRIu32, this->samples_read_,
             this->samples_to_stabilize_, this->voc_index_);
  }
}

void SGP4xComponent::measure_raw_() {
  float humidity = NAN;
  static uint32_t nox_conditioning_start = millis();

  if (!this->self_test_complete_) {
    ESP_LOGD(TAG, "Self-test not yet complete");
    return;
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

  uint16_t command;
  uint16_t data[2];
  size_t response_words;
  // Use SGP40 measure command if we don't care about NOx
  if (nox_sensor_ == nullptr) {
    command = SGP40_CMD_MEASURE_RAW;
    response_words = 1;
  } else {
    // SGP41 sensor must use NOx conditioning command for the first 10 seconds
    if (millis() - nox_conditioning_start < 10000) {
      command = SGP41_CMD_NOX_CONDITIONING;
      response_words = 1;
    } else {
      command = SGP41_CMD_MEASURE_RAW;
      response_words = 2;
    }
  }
  uint16_t rhticks = llround((uint16_t) ((humidity * 65535) / 100));
  uint16_t tempticks = (uint16_t) (((temperature + 45) * 65535) / 175);
  // first parameter are the relative humidity ticks
  data[0] = rhticks;
  // secomd parameter are the temperature ticks
  data[1] = tempticks;

  if (!this->write_command(command, data, 2)) {
    ESP_LOGD(TAG, "write error (%d)", this->last_error_);
    this->status_set_warning("measurement request failed");
    return;
  }

  this->set_timeout(this->measure_time_, [this, response_words]() {
    uint16_t raw_data[2];
    raw_data[1] = 0;
    if (!this->read_data(raw_data, response_words)) {
      ESP_LOGD(TAG, "read error (%d)", this->last_error_);
      this->status_set_warning("measurement read failed");
      this->voc_index_ = this->nox_index_ = UINT16_MAX;
      return;
    }
    this->voc_sraw_ = raw_data[0];
    this->nox_sraw_ = raw_data[1];  // either 0 or the measured NOx ticks
    this->status_clear_warning();
    this->update_gas_indices_();
  });
}

void SGP4xComponent::take_sample() {
  if (!this->self_test_complete_)
    return;
  this->seconds_since_last_store_ += 1;
  this->measure_raw_();
}

void SGP4xComponent::update() {
  if (this->samples_read_ < this->samples_to_stabilize_) {
    return;
  }
  if (this->voc_sensor_ != nullptr) {
    if (this->voc_index_ != UINT16_MAX)
      this->voc_sensor_->publish_state(this->voc_index_);
  }
  if (this->nox_sensor_ != nullptr) {
    if (this->nox_index_ != UINT16_MAX)
      this->nox_sensor_->publish_state(this->nox_index_);
  }
}

void SGP4xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SGP4x:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  store_baseline: %d", this->store_baseline_);

  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case SERIAL_NUMBER_IDENTIFICATION_FAILED:
        ESP_LOGW(TAG, "Get Serial number failed.");
        break;
      case SELF_TEST_FAILED:
        ESP_LOGW(TAG, "Self test failed.");
        break;

      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  } else {
    ESP_LOGCONFIG(TAG, "  Type: %s", sgp_type_ == SGP41 ? "SGP41" : "SPG40");
    ESP_LOGCONFIG(TAG, "  Serial number: %" PRIu64, this->serial_number_);
    ESP_LOGCONFIG(TAG, "  Minimum Samples: %f", GasIndexAlgorithm_INITIAL_BLACKOUT);
  }
  LOG_UPDATE_INTERVAL(this);

  if (this->humidity_sensor_ != nullptr || this->temperature_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Compensation:");
    LOG_SENSOR("    ", "Temperature Source:", this->temperature_sensor_);
    LOG_SENSOR("    ", "Humidity Source:", this->humidity_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Compensation: No source configured");
  }
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);
  LOG_SENSOR("  ", "NOx", this->nox_sensor_);
}

}  // namespace sgp4x
}  // namespace esphome

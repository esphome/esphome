#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_BSEC2
#include "bme68x_bsec2.h"

#include <string>

namespace esphome {
namespace bme68x_bsec2 {

#define BME68X_BSEC2_ALGORITHM_OUTPUT_LOG(a) (a == ALGORITHM_OUTPUT_CLASSIFICATION ? "Classification" : "Regression")
#define BME68X_BSEC2_OPERATING_AGE_LOG(o) (o == OPERATING_AGE_4D ? "4 days" : "28 days")
#define BME68X_BSEC2_SAMPLE_RATE_LOG(r) (r == SAMPLE_RATE_DEFAULT ? "Default" : (r == SAMPLE_RATE_ULP ? "ULP" : "LP"))
#define BME68X_BSEC2_VOLTAGE_LOG(v) (v == VOLTAGE_3_3V ? "3.3V" : "1.8V")

static const char *const TAG = "bme68x_bsec2.sensor";

static const std::string IAQ_ACCURACY_STATES[4] = {"Stabilizing", "Uncertain", "Calibrating", "Calibrated"};

void BME68xBSEC2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BME68X via BSEC2...");

  this->bsec_status_ = bsec_init_m(&this->bsec_instance_);
  if (this->bsec_status_ != BSEC_OK) {
    this->mark_failed();
    ESP_LOGE(TAG, "bsec_init_m failed: status %d", this->bsec_status_);
    return;
  }

  bsec_get_version_m(&this->bsec_instance_, &this->version_);

  this->bme68x_status_ = bme68x_init(&this->bme68x_);
  if (this->bme68x_status_ != BME68X_OK) {
    this->mark_failed();
    ESP_LOGE(TAG, "bme68x_init failed: status %d", this->bme68x_status_);
    return;
  }
  if (this->bsec2_configuration_ != nullptr && this->bsec2_configuration_length_) {
    this->set_config_(this->bsec2_configuration_, this->bsec2_configuration_length_);
    if (this->bsec_status_ != BSEC_OK) {
      this->mark_failed();
      ESP_LOGE(TAG, "bsec_set_configuration_m failed: status %d", this->bsec_status_);
      return;
    }
  }

  this->update_subscription_();
  if (this->bsec_status_ != BSEC_OK) {
    this->mark_failed();
    ESP_LOGE(TAG, "bsec_update_subscription_m failed: status %d", this->bsec_status_);
    return;
  }

  this->load_state_();
}

void BME68xBSEC2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BME68X via BSEC2:");

  ESP_LOGCONFIG(TAG, "  BSEC2 version: %d.%d.%d.%d", this->version_.major, this->version_.minor,
                this->version_.major_bugfix, this->version_.minor_bugfix);

  ESP_LOGCONFIG(TAG, "  BSEC2 configuration blob:");
  ESP_LOGCONFIG(TAG, "    Configured: %s", YESNO(this->bsec2_blob_configured_));
  if (this->bsec2_configuration_ != nullptr && this->bsec2_configuration_length_) {
    ESP_LOGCONFIG(TAG, "    Size: %" PRIu32, this->bsec2_configuration_length_);
  }

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication failed (BSEC2 status: %d, BME68X status: %d)", this->bsec_status_,
             this->bme68x_status_);
  }

  if (this->algorithm_output_ != ALGORITHM_OUTPUT_IAQ) {
    ESP_LOGCONFIG(TAG, "  Algorithm output: %s", BME68X_BSEC2_ALGORITHM_OUTPUT_LOG(this->algorithm_output_));
  }
  ESP_LOGCONFIG(TAG, "  Operating age: %s", BME68X_BSEC2_OPERATING_AGE_LOG(this->operating_age_));
  ESP_LOGCONFIG(TAG, "  Sample rate: %s", BME68X_BSEC2_SAMPLE_RATE_LOG(this->sample_rate_));
  ESP_LOGCONFIG(TAG, "  Voltage: %s", BME68X_BSEC2_VOLTAGE_LOG(this->voltage_));
  ESP_LOGCONFIG(TAG, "  State save interval: %ims", this->state_save_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Temperature offset: %.2f", this->temperature_offset_);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  ESP_LOGCONFIG(TAG, "    Sample rate: %s", BME68X_BSEC2_SAMPLE_RATE_LOG(this->temperature_sample_rate_));
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  ESP_LOGCONFIG(TAG, "    Sample rate: %s", BME68X_BSEC2_SAMPLE_RATE_LOG(this->pressure_sample_rate_));
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  ESP_LOGCONFIG(TAG, "    Sample rate: %s", BME68X_BSEC2_SAMPLE_RATE_LOG(this->humidity_sample_rate_));
  LOG_SENSOR("  ", "Gas resistance", this->gas_resistance_sensor_);
  LOG_SENSOR("  ", "CO2 equivalent", this->co2_equivalent_sensor_);
  LOG_SENSOR("  ", "Breath VOC equivalent", this->breath_voc_equivalent_sensor_);
  LOG_SENSOR("  ", "IAQ", this->iaq_sensor_);
  LOG_SENSOR("  ", "IAQ static", this->iaq_static_sensor_);
  LOG_SENSOR("  ", "Numeric IAQ accuracy", this->iaq_accuracy_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "IAQ accuracy", this->iaq_accuracy_text_sensor_);
#endif
}

float BME68xBSEC2Component::get_setup_priority() const { return setup_priority::DATA; }

void BME68xBSEC2Component::loop() {
  this->run_();

  if (this->bsec_status_ < BSEC_OK || this->bme68x_status_ < BME68X_OK) {
    this->status_set_error();
  } else {
    this->status_clear_error();
  }
  if (this->bsec_status_ > BSEC_OK || this->bme68x_status_ > BME68X_OK) {
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }
  // Process a single action from the queue. These are primarily sensor state publishes
  // that in totality take too long to send in a single call.
  if (this->queue_.size()) {
    auto action = std::move(this->queue_.front());
    this->queue_.pop();
    action();
  }
}

void BME68xBSEC2Component::set_config_(const uint8_t *config, uint32_t len) {
  if (len > BSEC_MAX_PROPERTY_BLOB_SIZE) {
    ESP_LOGE(TAG, "Configuration is larger than BSEC_MAX_PROPERTY_BLOB_SIZE");
    this->mark_failed();
    return;
  }
  uint8_t work_buffer[BSEC_MAX_PROPERTY_BLOB_SIZE];
  this->bsec_status_ = bsec_set_configuration_m(&this->bsec_instance_, config, len, work_buffer, sizeof(work_buffer));
  if (this->bsec_status_ == BSEC_OK) {
    this->bsec2_blob_configured_ = true;
  }
}

float BME68xBSEC2Component::calc_sensor_sample_rate_(SampleRate sample_rate) {
  if (sample_rate == SAMPLE_RATE_DEFAULT) {
    sample_rate = this->sample_rate_;
  }
  return sample_rate == SAMPLE_RATE_ULP ? BSEC_SAMPLE_RATE_ULP : BSEC_SAMPLE_RATE_LP;
}

void BME68xBSEC2Component::update_subscription_() {
  bsec_sensor_configuration_t virtual_sensors[BSEC_NUMBER_OUTPUTS];
  uint8_t num_virtual_sensors = 0;
#ifdef USE_SENSOR
  if (this->iaq_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_IAQ;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(SAMPLE_RATE_DEFAULT);
    num_virtual_sensors++;
  }

  if (this->iaq_static_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_STATIC_IAQ;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(SAMPLE_RATE_DEFAULT);
    num_virtual_sensors++;
  }

  if (this->co2_equivalent_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_CO2_EQUIVALENT;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(SAMPLE_RATE_DEFAULT);
    num_virtual_sensors++;
  }

  if (this->breath_voc_equivalent_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(SAMPLE_RATE_DEFAULT);
    num_virtual_sensors++;
  }

  if (this->pressure_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_RAW_PRESSURE;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(this->pressure_sample_rate_);
    num_virtual_sensors++;
  }

  if (this->gas_resistance_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_RAW_GAS;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(SAMPLE_RATE_DEFAULT);
    num_virtual_sensors++;
  }

  if (this->temperature_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(this->temperature_sample_rate_);
    num_virtual_sensors++;
  }

  if (this->humidity_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY;
    virtual_sensors[num_virtual_sensors].sample_rate = this->calc_sensor_sample_rate_(this->humidity_sample_rate_);
    num_virtual_sensors++;
  }
#endif
  bsec_sensor_configuration_t sensor_settings[BSEC_MAX_PHYSICAL_SENSOR];
  uint8_t num_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;
  this->bsec_status_ = bsec_update_subscription_m(&this->bsec_instance_, virtual_sensors, num_virtual_sensors,
                                                  sensor_settings, &num_sensor_settings);
}

void BME68xBSEC2Component::run_() {
  this->op_mode_ = this->bsec_settings_.op_mode;
  int64_t curr_time_ns = this->get_time_ns_();
  if (curr_time_ns < this->bsec_settings_.next_call) {
    return;
  }
  uint8_t status;

  ESP_LOGV(TAG, "Performing sensor run");

  struct bme68x_conf bme68x_conf;
  this->bsec_status_ = bsec_sensor_control_m(&this->bsec_instance_, curr_time_ns, &this->bsec_settings_);
  if (this->bsec_status_ < BSEC_OK) {
    ESP_LOGW(TAG, "Failed to fetch sensor control settings (BSEC2 error code %d)", this->bsec_status_);
    return;
  }

  switch (this->bsec_settings_.op_mode) {
    case BME68X_FORCED_MODE:
      bme68x_get_conf(&bme68x_conf, &this->bme68x_);

      bme68x_conf.os_hum = this->bsec_settings_.humidity_oversampling;
      bme68x_conf.os_temp = this->bsec_settings_.temperature_oversampling;
      bme68x_conf.os_pres = this->bsec_settings_.pressure_oversampling;
      bme68x_set_conf(&bme68x_conf, &this->bme68x_);
      this->bme68x_heatr_conf_.enable = BME68X_ENABLE;
      this->bme68x_heatr_conf_.heatr_temp = this->bsec_settings_.heater_temperature;
      this->bme68x_heatr_conf_.heatr_dur = this->bsec_settings_.heater_duration;

      // status = bme68x_set_op_mode(this->bsec_settings_.op_mode, &this->bme68x_);
      status = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &this->bme68x_heatr_conf_, &this->bme68x_);
      status = bme68x_set_op_mode(BME68X_FORCED_MODE, &this->bme68x_);
      this->op_mode_ = BME68X_FORCED_MODE;
      ESP_LOGV(TAG, "Using forced mode");

      break;
    case BME68X_PARALLEL_MODE:
      if (this->op_mode_ != this->bsec_settings_.op_mode) {
        bme68x_get_conf(&bme68x_conf, &this->bme68x_);

        bme68x_conf.os_hum = this->bsec_settings_.humidity_oversampling;
        bme68x_conf.os_temp = this->bsec_settings_.temperature_oversampling;
        bme68x_conf.os_pres = this->bsec_settings_.pressure_oversampling;
        bme68x_set_conf(&bme68x_conf, &this->bme68x_);

        this->bme68x_heatr_conf_.enable = BME68X_ENABLE;
        this->bme68x_heatr_conf_.heatr_temp_prof = this->bsec_settings_.heater_temperature_profile;
        this->bme68x_heatr_conf_.heatr_dur_prof = this->bsec_settings_.heater_duration_profile;
        this->bme68x_heatr_conf_.profile_len = this->bsec_settings_.heater_profile_len;
        this->bme68x_heatr_conf_.shared_heatr_dur =
            BSEC_TOTAL_HEAT_DUR -
            (bme68x_get_meas_dur(BME68X_PARALLEL_MODE, &bme68x_conf, &this->bme68x_) / INT64_C(1000));

        status = bme68x_set_heatr_conf(BME68X_PARALLEL_MODE, &this->bme68x_heatr_conf_, &this->bme68x_);

        status = bme68x_set_op_mode(BME68X_PARALLEL_MODE, &this->bme68x_);
        this->op_mode_ = BME68X_PARALLEL_MODE;
        ESP_LOGV(TAG, "Using parallel mode");
      }
      break;
    case BME68X_SLEEP_MODE:
      if (this->op_mode_ != this->bsec_settings_.op_mode) {
        bme68x_set_op_mode(BME68X_SLEEP_MODE, &this->bme68x_);
        this->op_mode_ = BME68X_SLEEP_MODE;
        ESP_LOGV(TAG, "Using sleep mode");
      }
      break;
  }

  if (this->bsec_settings_.trigger_measurement && this->bsec_settings_.op_mode != BME68X_SLEEP_MODE) {
    uint32_t meas_dur = 0;
    meas_dur = bme68x_get_meas_dur(this->op_mode_, &bme68x_conf, &this->bme68x_);
    ESP_LOGV(TAG, "Queueing read in %uus", meas_dur);
    this->set_timeout("read", meas_dur / 1000, [this, curr_time_ns]() { this->read_(curr_time_ns); });
  } else {
    ESP_LOGV(TAG, "Measurement not required");
    this->read_(curr_time_ns);
  }
}

void BME68xBSEC2Component::read_(int64_t trigger_time_ns) {
  ESP_LOGV(TAG, "Reading data");

  if (this->bsec_settings_.trigger_measurement) {
    uint8_t current_op_mode;
    this->bme68x_status_ = bme68x_get_op_mode(&current_op_mode, &this->bme68x_);

    if (current_op_mode == BME68X_SLEEP_MODE) {
      ESP_LOGV(TAG, "Still in sleep mode, doing nothing");
      return;
    }
  }

  if (!this->bsec_settings_.process_data) {
    ESP_LOGV(TAG, "Data processing not required");
    return;
  }

  struct bme68x_data data[3];
  uint8_t nFields = 0;
  this->bme68x_status_ = bme68x_get_data(this->op_mode_, &data[0], &nFields, &this->bme68x_);

  if (this->bme68x_status_ != BME68X_OK) {
    ESP_LOGW(TAG, "Failed to get sensor data (BME68X error code %d)", this->bme68x_status_);
    return;
  }
  if (nFields < 1) {
    ESP_LOGD(TAG, "BME68X did not provide new data");
    return;
  }

  for (uint8_t i = 0; i < nFields; i++) {
    bsec_input_t inputs[BSEC_MAX_PHYSICAL_SENSOR];  // Temperature, Pressure, Humidity & Gas Resistance
    uint8_t num_inputs = 0;

    if (BSEC_CHECK_INPUT(this->bsec_settings_.process_data, BSEC_INPUT_TEMPERATURE)) {
      inputs[num_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
      inputs[num_inputs].signal = data[i].temperature;
      inputs[num_inputs].time_stamp = trigger_time_ns;
      num_inputs++;
    }
    if (BSEC_CHECK_INPUT(this->bsec_settings_.process_data, BSEC_INPUT_HEATSOURCE)) {
      inputs[num_inputs].sensor_id = BSEC_INPUT_HEATSOURCE;
      inputs[num_inputs].signal = this->temperature_offset_;
      inputs[num_inputs].time_stamp = trigger_time_ns;
      num_inputs++;
    }
    if (BSEC_CHECK_INPUT(this->bsec_settings_.process_data, BSEC_INPUT_HUMIDITY)) {
      inputs[num_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
      inputs[num_inputs].signal = data[i].humidity;
      inputs[num_inputs].time_stamp = trigger_time_ns;
      num_inputs++;
    }
    if (BSEC_CHECK_INPUT(this->bsec_settings_.process_data, BSEC_INPUT_PRESSURE)) {
      inputs[num_inputs].sensor_id = BSEC_INPUT_PRESSURE;
      inputs[num_inputs].signal = data[i].pressure;
      inputs[num_inputs].time_stamp = trigger_time_ns;
      num_inputs++;
    }
    if (BSEC_CHECK_INPUT(this->bsec_settings_.process_data, BSEC_INPUT_GASRESISTOR)) {
      if (data[i].status & BME68X_GASM_VALID_MSK) {
        inputs[num_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
        inputs[num_inputs].signal = data[i].gas_resistance;
        inputs[num_inputs].time_stamp = trigger_time_ns;
        num_inputs++;
      } else {
        ESP_LOGD(TAG, "BME68X did not report gas data");
      }
    }
    if (BSEC_CHECK_INPUT(this->bsec_settings_.process_data, BSEC_INPUT_PROFILE_PART) &&
        (data[i].status & BME68X_GASM_VALID_MSK)) {
      inputs[num_inputs].sensor_id = BSEC_INPUT_PROFILE_PART;
      inputs[num_inputs].signal = (this->op_mode_ == BME68X_FORCED_MODE) ? 0 : data[i].gas_index;
      inputs[num_inputs].time_stamp = trigger_time_ns;
      num_inputs++;
    }

    if (num_inputs < 1) {
      ESP_LOGD(TAG, "No signal inputs available for BSEC2");
      return;
    }

    bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
    uint8_t num_outputs = BSEC_NUMBER_OUTPUTS;
    this->bsec_status_ = bsec_do_steps_m(&this->bsec_instance_, inputs, num_inputs, outputs, &num_outputs);
    if (this->bsec_status_ != BSEC_OK) {
      ESP_LOGW(TAG, "BSEC2 failed to process signals (BSEC2 error code %d)", this->bsec_status_);
      return;
    }
    if (num_outputs < 1) {
      ESP_LOGD(TAG, "No signal outputs provided by BSEC2");
      return;
    }

    this->publish_(outputs, num_outputs);
  }
}

void BME68xBSEC2Component::publish_(const bsec_output_t *outputs, uint8_t num_outputs) {
  ESP_LOGV(TAG, "Publishing sensor states");
  bool update_accuracy = false;
  uint8_t max_accuracy = 0;
  for (uint8_t i = 0; i < num_outputs; i++) {
    float signal = outputs[i].signal;
    switch (outputs[i].sensor_id) {
      case BSEC_OUTPUT_IAQ:
        max_accuracy = std::max(outputs[i].accuracy, max_accuracy);
        update_accuracy = true;
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->iaq_sensor_, signal); });
#endif
        break;
      case BSEC_OUTPUT_STATIC_IAQ:
        max_accuracy = std::max(outputs[i].accuracy, max_accuracy);
        update_accuracy = true;
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->iaq_static_sensor_, signal); });
#endif
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->co2_equivalent_sensor_, signal); });
#endif
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->breath_voc_equivalent_sensor_, signal); });
#endif
        break;
      case BSEC_OUTPUT_RAW_PRESSURE:
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->pressure_sensor_, signal / 100.0f); });
#endif
        break;
      case BSEC_OUTPUT_RAW_GAS:
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->gas_resistance_sensor_, signal); });
#endif
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->temperature_sensor_, signal); });
#endif
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
#ifdef USE_SENSOR
        this->queue_push_([this, signal]() { this->publish_sensor_(this->humidity_sensor_, signal); });
#endif
        break;
    }
  }
  if (update_accuracy) {
#ifdef USE_SENSOR
    this->queue_push_(
        [this, max_accuracy]() { this->publish_sensor_(this->iaq_accuracy_sensor_, max_accuracy, true); });
#endif
#ifdef USE_TEXT_SENSOR
    this->queue_push_([this, max_accuracy]() {
      this->publish_sensor_(this->iaq_accuracy_text_sensor_, IAQ_ACCURACY_STATES[max_accuracy]);
    });
#endif
    // Queue up an opportunity to save state
    this->queue_push_([this, max_accuracy]() { this->save_state_(max_accuracy); });
  }
}

int64_t BME68xBSEC2Component::get_time_ns_() {
  int64_t time_ms = millis();
  if (this->last_time_ms_ > time_ms) {
    this->millis_overflow_counter_++;
  }
  this->last_time_ms_ = time_ms;

  return (time_ms + ((int64_t) this->millis_overflow_counter_ << 32)) * INT64_C(1000000);
}

#ifdef USE_SENSOR
void BME68xBSEC2Component::publish_sensor_(sensor::Sensor *sensor, float value, bool change_only) {
  if (!sensor || (change_only && sensor->has_state() && sensor->state == value)) {
    return;
  }
  sensor->publish_state(value);
}
#endif

#ifdef USE_TEXT_SENSOR
void BME68xBSEC2Component::publish_sensor_(text_sensor::TextSensor *sensor, const std::string &value) {
  if (!sensor || (sensor->has_state() && sensor->state == value)) {
    return;
  }
  sensor->publish_state(value);
}
#endif

void BME68xBSEC2Component::load_state_() {
  uint32_t hash = this->get_hash();
  this->bsec_state_ = global_preferences->make_preference<uint8_t[BSEC_MAX_STATE_BLOB_SIZE]>(hash, true);

  uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
  if (this->bsec_state_.load(&state)) {
    ESP_LOGV(TAG, "Loading state");
    uint8_t work_buffer[BSEC_MAX_WORKBUFFER_SIZE];
    this->bsec_status_ =
        bsec_set_state_m(&this->bsec_instance_, state, BSEC_MAX_STATE_BLOB_SIZE, work_buffer, sizeof(work_buffer));
    if (this->bsec_status_ != BSEC_OK) {
      ESP_LOGW(TAG, "Failed to load state (BSEC2 error code %d)", this->bsec_status_);
    }
    ESP_LOGI(TAG, "Loaded state");
  }
}

void BME68xBSEC2Component::save_state_(uint8_t accuracy) {
  if (accuracy < 3 || (millis() - this->last_state_save_ms_ < this->state_save_interval_ms_)) {
    return;
  }

  ESP_LOGV(TAG, "Saving state");

  uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
  uint8_t work_buffer[BSEC_MAX_STATE_BLOB_SIZE];
  uint32_t num_serialized_state = BSEC_MAX_STATE_BLOB_SIZE;

  this->bsec_status_ = bsec_get_state_m(&this->bsec_instance_, 0, state, BSEC_MAX_STATE_BLOB_SIZE, work_buffer,
                                        BSEC_MAX_STATE_BLOB_SIZE, &num_serialized_state);
  if (this->bsec_status_ != BSEC_OK) {
    ESP_LOGW(TAG, "Failed fetch state for save (BSEC2 error code %d)", this->bsec_status_);
    return;
  }

  if (!this->bsec_state_.save(&state)) {
    ESP_LOGW(TAG, "Failed to save state");
    return;
  }
  this->last_state_save_ms_ = millis();

  ESP_LOGI(TAG, "Saved state");
}

}  // namespace bme68x_bsec2
}  // namespace esphome
#endif

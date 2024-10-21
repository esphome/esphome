#include "bme680_bsec.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <string>

namespace esphome {
namespace bme680_bsec {
#ifdef USE_BSEC
static const char *const TAG = "bme680_bsec.sensor";

static const std::string IAQ_ACCURACY_STATES[4] = {"Stabilizing", "Uncertain", "Calibrating", "Calibrated"};

std::vector<BME680BSECComponent *>
    BME680BSECComponent::instances;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
uint8_t BME680BSECComponent::work_buffer_[BSEC_MAX_WORKBUFFER_SIZE] = {0};

void BME680BSECComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BME68x(%s) via BSEC...", this->device_id_.c_str());

  uint8_t new_idx = BME680BSECComponent::instances.size();
  BME680BSECComponent::instances.push_back(this);

  this->bsec_state_data_valid_ = false;

  // Initialize the bme68x_ structure (passed-in to the bme68x_* functions) and the BME68x device
  this->bme68x_.intf_ptr =
      this;  // This will be passed-in as last parameter to the next "read" and "write" function pointers.
  this->bme68x_.intf = BME68X_I2C_INTF;  // We currently only support I2C interface
  this->bme68x_.read = BME680BSECComponent::read_bytes_wrapper;
  this->bme68x_.write = BME680BSECComponent::write_bytes_wrapper;
  this->bme68x_.delay_us = BME680BSECComponent::delay_us;
  this->bme68x_.amb_temp = 25;

  this->bme68x_status_ = bme68x_init(&this->bme68x_);
  if (this->bme68x_status_ != BME68X_OK) {
    this->mark_failed();
    return;
  }

  // Initialize the BSEC library
  if (this->reinit_bsec_lib_() != 0) {
    this->mark_failed();
    return;
  }

  // Load the BSEC library state from storage
  this->load_state_();
}

void BME680BSECComponent::set_config_() {
  if (this->sample_rate_ == SAMPLE_RATE_ULP) {
    if (this->supply_voltage_ == SUPPLY_VOLTAGE_3V3) {
      const uint8_t config[] = {
#include "config/generic_33v_300s_28d/bsec_iaq.txt"
      };
      this->bsec_status_ =
          bsec_set_configuration(config, BSEC_MAX_PROPERTY_BLOB_SIZE, this->work_buffer_, sizeof(this->work_buffer_));
    } else {  // SUPPLY_VOLTAGE_1V8
      const uint8_t config[] = {
#include "config/generic_18v_300s_28d/bsec_iaq.txt"
      };
      this->bsec_status_ =
          bsec_set_configuration(config, BSEC_MAX_PROPERTY_BLOB_SIZE, this->work_buffer_, sizeof(this->work_buffer_));
    }
  } else {  // SAMPLE_RATE_LP
    if (this->supply_voltage_ == SUPPLY_VOLTAGE_3V3) {
      const uint8_t config[] = {
#include "config/generic_33v_3s_28d/bsec_iaq.txt"
      };
      this->bsec_status_ =
          bsec_set_configuration(config, BSEC_MAX_PROPERTY_BLOB_SIZE, this->work_buffer_, sizeof(this->work_buffer_));
    } else {  // SUPPLY_VOLTAGE_1V8
      const uint8_t config[] = {
#include "config/generic_18v_3s_28d/bsec_iaq.txt"
      };
      this->bsec_status_ =
          bsec_set_configuration(config, BSEC_MAX_PROPERTY_BLOB_SIZE, this->work_buffer_, sizeof(this->work_buffer_));
    }
  }
}

float BME680BSECComponent::calc_sensor_sample_rate_(SampleRate sample_rate) {
  if (sample_rate == SAMPLE_RATE_DEFAULT) {
    sample_rate = this->sample_rate_;
  }
  return sample_rate == SAMPLE_RATE_ULP ? BSEC_SAMPLE_RATE_ULP : BSEC_SAMPLE_RATE_LP;
}

void BME680BSECComponent::update_subscription_() {
  bsec_sensor_configuration_t virtual_sensors[BSEC_NUMBER_OUTPUTS];
  int num_virtual_sensors = 0;

  if (this->iaq_sensor_) {
    virtual_sensors[num_virtual_sensors].sensor_id =
        this->iaq_mode_ == IAQ_MODE_STATIC ? BSEC_OUTPUT_STATIC_IAQ : BSEC_OUTPUT_IAQ;
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

  bsec_sensor_configuration_t sensor_settings[BSEC_MAX_PHYSICAL_SENSOR];
  uint8_t num_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;
  this->bsec_status_ =
      bsec_update_subscription(virtual_sensors, num_virtual_sensors, sensor_settings, &num_sensor_settings);
  ESP_LOGV(TAG, "%s: updating subscription for %d virtual sensors (out=%d sensors)", this->device_id_.c_str(),
           num_virtual_sensors, num_sensor_settings);
}

void BME680BSECComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "%s via BSEC:", this->device_id_.c_str());

  bsec_version_t version;
  bsec_get_version(&version);
  ESP_LOGCONFIG(TAG, "  BSEC Version: %d.%d.%d.%d", version.major, version.minor, version.major_bugfix,
                version.minor_bugfix);

  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication failed (BSEC Status: %d, BME68x Status: %d)", this->bsec_status_,
             this->bme68x_status_);
  }

  ESP_LOGCONFIG(TAG, "  Temperature Offset: %.2f", this->temperature_offset_);
  ESP_LOGCONFIG(TAG, "  IAQ Mode: %s", this->iaq_mode_ == IAQ_MODE_STATIC ? "Static" : "Mobile");
  ESP_LOGCONFIG(TAG, "  Supply Voltage: %sV", this->supply_voltage_ == SUPPLY_VOLTAGE_3V3 ? "3.3" : "1.8");
  ESP_LOGCONFIG(TAG, "  Sample Rate: %s", BME680_BSEC_SAMPLE_RATE_LOG(this->sample_rate_));
  ESP_LOGCONFIG(TAG, "  State Save Interval: %ims", this->state_save_interval_ms_);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  ESP_LOGCONFIG(TAG, "    Sample Rate: %s", BME680_BSEC_SAMPLE_RATE_LOG(this->temperature_sample_rate_));
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  ESP_LOGCONFIG(TAG, "    Sample Rate: %s", BME680_BSEC_SAMPLE_RATE_LOG(this->pressure_sample_rate_));
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  ESP_LOGCONFIG(TAG, "    Sample Rate: %s", BME680_BSEC_SAMPLE_RATE_LOG(this->humidity_sample_rate_));
  LOG_SENSOR("  ", "Gas Resistance", this->gas_resistance_sensor_);
  LOG_SENSOR("  ", "IAQ", this->iaq_sensor_);
  LOG_SENSOR("  ", "Numeric IAQ Accuracy", this->iaq_accuracy_sensor_);
  LOG_TEXT_SENSOR("  ", "IAQ Accuracy", this->iaq_accuracy_text_sensor_);
  LOG_SENSOR("  ", "CO2 Equivalent", this->co2_equivalent_sensor_);
  LOG_SENSOR("  ", "Breath VOC Equivalent", this->breath_voc_equivalent_sensor_);
}

float BME680BSECComponent::get_setup_priority() const { return setup_priority::DATA; }

void BME680BSECComponent::loop() {
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

void BME680BSECComponent::run_() {
  int64_t curr_time_ns = this->get_time_ns_();
  if (curr_time_ns < this->next_call_ns_) {
    return;
  }

  ESP_LOGV(TAG, "%s: Performing sensor run", this->device_id_.c_str());

  // Restore BSEC library state
  // The reinit_bsec_lib_ method is computationally expensive: it takes 1200÷2900 microseconds on a ESP32.
  // It can be skipped entirely when there is only one device (since the BSEC library won't be shared)
  if (BME680BSECComponent::instances.size() > 1) {
    int res = this->reinit_bsec_lib_();
    if (res != 0)
      return;
  }

  this->bsec_status_ = bsec_sensor_control(curr_time_ns, &this->bseclib_settings_);
  if (this->bsec_status_ < BSEC_OK) {
    ESP_LOGW(TAG, "Failed to fetch sensor control settings (BSEC Error Code %d)", this->bsec_status_);
    return;
  }
  this->next_call_ns_ = this->bseclib_settings_.next_call;

  if (this->bseclib_settings_.trigger_measurement) {
    // Time to perform a new measurement

    // Set general sensor configuration
    struct bme68x_conf bme68x_general_conf;
    bme68x_general_conf.os_temp = this->bseclib_settings_.temperature_oversampling;
    bme68x_general_conf.os_pres = this->bseclib_settings_.pressure_oversampling;
    bme68x_general_conf.os_hum = this->bseclib_settings_.humidity_oversampling;
    bme68x_general_conf.filter = BME68X_FILTER_OFF;
    bme68x_general_conf.odr = BME68X_ODR_NONE;
    this->bme68x_status_ = bme68x_set_conf(&bme68x_general_conf, &this->bme68x_);
    if (this->bme68x_status_ != BME68X_OK) {
      ESP_LOGW(TAG, "Failed to set general sensor config (BME68x Error Code %d)", this->bme68x_status_);
      return;
    }

    // Set heater configuration
    struct bme68x_heatr_conf bme68x_heater_conf;
    bme68x_heater_conf.enable = this->bseclib_settings_.run_gas;
    bme68x_heater_conf.heatr_temp = this->bseclib_settings_.heater_temperature;
    bme68x_heater_conf.heatr_dur = this->bseclib_settings_.heating_duration;
    this->bme68x_status_ = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &bme68x_heater_conf, &this->bme68x_);
    if (this->bme68x_status_ != BME68X_OK) {
      ESP_LOGW(TAG, "Failed to set gas sensor config (BME68x Error Code %d)", this->bme68x_status_);
      return;
    }

    // Set sensor operation mode
    this->bme68x_status_ = bme68x_set_op_mode(BME68X_FORCED_MODE, &this->bme68x_);
    if (this->bme68x_status_ != BME68X_OK) {
      ESP_LOGW(TAG, "Failed to set sensor mode (BME68x Error Code %d)", this->bme68x_status_);
      return;
    }

    // Calculate time for the measurement to be ready
    uint32_t meas_dur_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &bme68x_general_conf, &this->bme68x_) +
                           bme68x_heater_conf.heatr_dur * 1000;

    // Since we are about to go "out of scope" in the loop, take a snapshot of the state now so we can restore it later
    // TODO: it would be interesting to see if this is really needed here, or if it's needed only after each
    // bsec_do_steps() call
    if (BME680BSECComponent::instances.size() > 1)
      this->snapshot_state_();

    // Queuing read for later, so ESPHome can do something more productive in the meantime
    uint32_t meas_dur = meas_dur_us / 1000;
    ESP_LOGV(TAG, "Queueing read in %ums", meas_dur);
    this->set_timeout("read", meas_dur, [this]() { this->read_(); });
  } else {
    ESP_LOGV(TAG, "Measurement not required");
    this->read_();
  }
}

void BME680BSECComponent::read_() {
  ESP_LOGV(TAG, "%s: Reading data", this->device_id_.c_str());
  int64_t curr_time_ns = this->get_time_ns_();

  if (this->bseclib_settings_.trigger_measurement) {
    uint8_t op_mode = BME68X_FORCED_MODE;
    while (op_mode != BME68X_SLEEP_MODE) {
      this->bme68x_status_ = bme68x_get_op_mode(&op_mode, &this->bme68x_);
      if (this->bme68x_status_ != BME68X_OK) {
        ESP_LOGW(TAG, "Failed to get sensor mode (BME68x Error Code %d)", this->bme68x_status_);
        return;
      }
    }
  }

  if (!this->bseclib_settings_.process_data) {
    ESP_LOGV(TAG, "Data processing not required");
    return;
  }

  struct bme68x_data data;
  uint8_t data_count = 0;
  this->bme68x_status_ = bme68x_get_data(BME68X_FORCED_MODE, &data, &data_count, &this->bme68x_);
  if (this->bme68x_status_ != BME68X_OK) {
    ESP_LOGW(TAG, "Failed to get sensor data (BME68x Error Code %d)", this->bme68x_status_);
    return;
  }
  if (!(data.status & BME68X_NEW_DATA_MSK)) {
    ESP_LOGD(TAG, "BME68x did not report new data");
    return;
  }

  bsec_input_t
      inputs[BSEC_MAX_PHYSICAL_SENSOR];  // Temperature, Temperature offset, Pressure, Humidity & Gas Resistance
  uint8_t num_inputs = 0;

  if (this->bseclib_settings_.process_data & BSEC_PROCESS_TEMPERATURE) {
    inputs[num_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
    inputs[num_inputs].signal = data.temperature;
    inputs[num_inputs].time_stamp = curr_time_ns;
    ESP_LOGV(TAG, "%s: BSEC In[%d](Temperature)=%f", this->device_id_.c_str(), num_inputs, inputs[num_inputs].signal);
    num_inputs++;

    // Temperature offset from the real temperature due to external heat sources
    inputs[num_inputs].sensor_id = BSEC_INPUT_HEATSOURCE;
    inputs[num_inputs].signal = this->temperature_offset_;
    inputs[num_inputs].time_stamp = curr_time_ns;
    ESP_LOGV(TAG, "%s: BSEC In[%d](TemperatureOffset)=%f", this->device_id_.c_str(), num_inputs,
             inputs[num_inputs].signal);
    num_inputs++;
  }
  if (this->bseclib_settings_.process_data & BSEC_PROCESS_HUMIDITY) {
    inputs[num_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
    inputs[num_inputs].signal =
        data.humidity;  // Comment in struct bme68x_data wrongly states that humidity unit is %RH x1000, but inspecting
                        // the data when using float type the unit is plain %RH
    inputs[num_inputs].time_stamp = curr_time_ns;
    ESP_LOGV(TAG, "%s: BSEC In[%d](Humidity)=%f", this->device_id_.c_str(), num_inputs, inputs[num_inputs].signal);
    num_inputs++;
  }
  if (this->bseclib_settings_.process_data & BSEC_PROCESS_PRESSURE) {
    inputs[num_inputs].sensor_id = BSEC_INPUT_PRESSURE;
    inputs[num_inputs].signal = data.pressure;
    inputs[num_inputs].time_stamp = curr_time_ns;
    ESP_LOGV(TAG, "%s: BSEC In[%d](Pressure)=%f", this->device_id_.c_str(), num_inputs, inputs[num_inputs].signal);
    num_inputs++;
  }
  if (this->bseclib_settings_.process_data & BSEC_PROCESS_GAS) {
    if (data.status & BME68X_GASM_VALID_MSK) {
      inputs[num_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
      inputs[num_inputs].signal = min(
          data.gas_resistance,
          12800000.0f);  // Limit the maximum value to avoid BSEC error -2 (BSEC_E_DOSTEPS_VALUELIMITS)
                         // Starting with clean calibration data, a sensor has been observed generating constant gas
                         // resistance values at 12917167, which then decreased over time. A signal value at 12800001.0f
                         // results in the BSEC library v1.8.1492 returning error -2 (BSEC_E_DOSTEPS_VALUELIMITS).
                         // 12800000 and below are OK; no problem at all in BSEC v1.6.1480
      inputs[num_inputs].time_stamp = curr_time_ns;
      ESP_LOGV(TAG, "%s: BSEC In[%d](GasResistance)=%f (original=%f)", this->device_id_.c_str(), num_inputs,
               inputs[num_inputs].signal, data.gas_resistance);
      num_inputs++;
    } else {
      ESP_LOGD(TAG, "BME68x did not report gas data");
    }
  }
  if (num_inputs < 1) {
    ESP_LOGD(TAG, "No signal inputs available for BSEC");
    return;
  }

  // Restore BSEC library state
  // The reinit_bsec_lib_ method is computationally expensive: it takes 1200÷2900 microseconds on a ESP32.
  // It can be skipped entirely when there is only one device (since the BSEC library won't be shared)
  if (BME680BSECComponent::instances.size() > 1) {
    int res = this->reinit_bsec_lib_();
    if (res != 0)
      return;
    // Now that the BSEC library has been re-initialized, bsec_sensor_control *NEEDS* to be called in order to support
    // multiple devices with a different set of enabled sensors (even if the bseclib_settings_ data is not used)
    this->bsec_status_ = bsec_sensor_control(curr_time_ns, &this->bseclib_settings_);
    if (this->bsec_status_ < BSEC_OK) {
      ESP_LOGW(TAG, "Failed to fetch sensor control settings (BSEC Error Code %d)", this->bsec_status_);
      return;
    }
  }

  bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
  uint8_t num_outputs = BSEC_NUMBER_OUTPUTS;
  this->bsec_status_ = bsec_do_steps(inputs, num_inputs, outputs, &num_outputs);
  if (this->bsec_status_ != BSEC_OK) {
    ESP_LOGW(TAG, "BSEC failed to process signals (BSEC Error Code %d)", this->bsec_status_);
    return;
  }
  ESP_LOGV(TAG, "%s: after bsec_do_steps: num_inputs=%d num_outputs=%d", this->device_id_.c_str(), num_inputs,
           num_outputs);

  // Since we are about to go "out of scope" in the loop, take a snapshot of the state now so we can restore it later
  if (BME680BSECComponent::instances.size() > 1)
    this->snapshot_state_();

  if (num_outputs < 1) {
    ESP_LOGD(TAG, "No signal outputs provided by BSEC");
    return;
  }

  this->publish_(outputs, num_outputs);
}

void BME680BSECComponent::publish_(const bsec_output_t *outputs, uint8_t num_outputs) {
  ESP_LOGV(TAG, "%s: Queuing sensor state publish actions", this->device_id_.c_str());
  for (uint8_t i = 0; i < num_outputs; i++) {
    float signal = outputs[i].signal;
    switch (outputs[i].sensor_id) {
      case BSEC_OUTPUT_IAQ:
      case BSEC_OUTPUT_STATIC_IAQ: {
        uint8_t accuracy = outputs[i].accuracy;
        this->queue_push_([this, signal]() { this->publish_sensor_(this->iaq_sensor_, signal); });
        this->queue_push_([this, accuracy]() {
          this->publish_sensor_(this->iaq_accuracy_text_sensor_, IAQ_ACCURACY_STATES[accuracy]);
        });
        this->queue_push_([this, accuracy]() { this->publish_sensor_(this->iaq_accuracy_sensor_, accuracy, true); });

        // Queue up an opportunity to save state
        this->queue_push_([this, accuracy]() { this->save_state_(accuracy); });
      } break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:
        this->queue_push_([this, signal]() { this->publish_sensor_(this->co2_equivalent_sensor_, signal); });
        break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        this->queue_push_([this, signal]() { this->publish_sensor_(this->breath_voc_equivalent_sensor_, signal); });
        break;
      case BSEC_OUTPUT_RAW_PRESSURE:
        this->queue_push_([this, signal]() { this->publish_sensor_(this->pressure_sensor_, signal / 100.0f); });
        break;
      case BSEC_OUTPUT_RAW_GAS:
        this->queue_push_([this, signal]() { this->publish_sensor_(this->gas_resistance_sensor_, signal); });
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        this->queue_push_([this, signal]() { this->publish_sensor_(this->temperature_sensor_, signal); });
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        this->queue_push_([this, signal]() { this->publish_sensor_(this->humidity_sensor_, signal); });
        break;
    }
  }
}

int64_t BME680BSECComponent::get_time_ns_() {
  int64_t time_ms = millis();
  if (this->last_time_ms_ > time_ms) {
    this->millis_overflow_counter_++;
  }
  this->last_time_ms_ = time_ms;

  return (time_ms + ((int64_t) this->millis_overflow_counter_ << 32)) * INT64_C(1000000);
}

void BME680BSECComponent::publish_sensor_(sensor::Sensor *sensor, float value, bool change_only) {
  if (!sensor || (change_only && sensor->has_state() && sensor->state == value)) {
    return;
  }
  sensor->publish_state(value);
}

void BME680BSECComponent::publish_sensor_(text_sensor::TextSensor *sensor, const std::string &value) {
  if (!sensor || (sensor->has_state() && sensor->state == value)) {
    return;
  }
  sensor->publish_state(value);
}

// Communication function - read
// Last parameter is a pointer to our BME680BSECComponent object, which is passed-back here as-is
BME68X_INTF_RET_TYPE BME680BSECComponent::read_bytes_wrapper(uint8_t reg_addr, uint8_t *reg_data, uint32_t len,
                                                             void *ptr) {
  BME680BSECComponent *inst = (BME680BSECComponent *) ptr;
  // Use the I2CDevice::read_bytes method to perform the actual I2C register read
  return inst->read_bytes(reg_addr, reg_data, len) ? 0 : -1;
}

// Communication function - write
// Last parameter is a pointer to our BME680BSECComponent object, which is passed-back here as-is
BME68X_INTF_RET_TYPE BME680BSECComponent::write_bytes_wrapper(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len,
                                                              void *ptr) {
  BME680BSECComponent *inst = (BME680BSECComponent *) ptr;
  // Use the I2CDevice::write_bytes method to perform the actual I2C register write
  return inst->write_bytes(reg_addr, reg_data, len) ? 0 : -1;
}

void BME680BSECComponent::delay_us(uint32_t period, void *ptr) {
  ESP_LOGV(TAG, "Delaying for %uus", period);
  delay_microseconds_safe(period);
}

// Fetch the BSEC library state and save it in the bsec_state_data_ member (volatile memory)
// Used to share the library when using more than one sensor
void BME680BSECComponent::snapshot_state_() {
  uint32_t num_serialized_state = BSEC_MAX_STATE_BLOB_SIZE;
  this->bsec_status_ = bsec_get_state(0, this->bsec_state_data_, BSEC_MAX_STATE_BLOB_SIZE, this->work_buffer_,
                                      sizeof(this->work_buffer_), &num_serialized_state);
  if (this->bsec_status_ != BSEC_OK) {
    ESP_LOGW(TAG, "%s: Failed to fetch BSEC library state for snapshot (BSEC Error Code %d)", this->device_id_.c_str(),
             this->bsec_status_);
    return;
  }
  this->bsec_state_data_valid_ = true;
}

// Restores the BSEC library state from a snapshot in memory
// Used to share the library when using more than one sensor
void BME680BSECComponent::restore_state_() {
  if (!this->bsec_state_data_valid_) {
    ESP_LOGV(TAG, "%s: BSEC state data NOT valid, aborting restore_state_()", this->device_id_.c_str());
    return;
  }

  this->bsec_status_ =
      bsec_set_state(this->bsec_state_data_, BSEC_MAX_STATE_BLOB_SIZE, this->work_buffer_, sizeof(this->work_buffer_));
  if (this->bsec_status_ != BSEC_OK) {
    ESP_LOGW(TAG, "Failed to restore BSEC library state (BSEC Error Code %d)", this->bsec_status_);
    return;
  }
}

int BME680BSECComponent::reinit_bsec_lib_() {
  this->bsec_status_ = bsec_init();
  if (this->bsec_status_ != BSEC_OK) {
    this->mark_failed();
    return -1;
  }

  this->set_config_();
  if (this->bsec_status_ != BSEC_OK) {
    this->mark_failed();
    return -2;
  }

  this->restore_state_();

  this->update_subscription_();
  if (this->bsec_status_ != BSEC_OK) {
    this->mark_failed();
    return -3;
  }

  return 0;
}

void BME680BSECComponent::load_state_() {
  uint32_t hash = fnv1_hash("bme68x_bsec_state_" + this->device_id_);
  this->bsec_state_ = global_preferences->make_preference<uint8_t[BSEC_MAX_STATE_BLOB_SIZE]>(hash, true);

  if (!this->bsec_state_.load(&this->bsec_state_data_)) {
    // No saved BSEC library state available
    return;
  }

  ESP_LOGV(TAG, "%s: Loading BSEC library state", this->device_id_.c_str());
  this->bsec_status_ =
      bsec_set_state(this->bsec_state_data_, BSEC_MAX_STATE_BLOB_SIZE, this->work_buffer_, sizeof(this->work_buffer_));
  if (this->bsec_status_ != BSEC_OK) {
    ESP_LOGW(TAG, "%s: Failed to load BSEC library state (BSEC Error Code %d)", this->device_id_.c_str(),
             this->bsec_status_);
    return;
  }
  // All OK: set the BSEC state data as valid
  this->bsec_state_data_valid_ = true;
  ESP_LOGI(TAG, "%s: Loaded BSEC library state", this->device_id_.c_str());
}

void BME680BSECComponent::save_state_(uint8_t accuracy) {
  if (accuracy < 3 || (millis() - this->last_state_save_ms_ < this->state_save_interval_ms_)) {
    return;
  }
  if (BME680BSECComponent::instances.size() <= 1) {
    // When a single device is in use, no snapshot is taken regularly so one is taken now
    // On multiple devices, a snapshot is taken at every loop, so there is no need to take one here
    this->snapshot_state_();
  }
  if (!this->bsec_state_data_valid_)
    return;

  ESP_LOGV(TAG, "%s: Saving state", this->device_id_.c_str());

  if (!this->bsec_state_.save(&this->bsec_state_data_)) {
    ESP_LOGW(TAG, "Failed to save state");
    return;
  }
  this->last_state_save_ms_ = millis();

  ESP_LOGI(TAG, "Saved state");
}
#endif
}  // namespace bme680_bsec
}  // namespace esphome

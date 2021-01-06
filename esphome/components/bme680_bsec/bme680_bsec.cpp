#ifdef USING_BSEC

#include "bme680_bsec.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <string>

namespace esphome {
namespace bme680_bsec {

static const char *TAG = "bme680_bsec.sensor";

static const std::string IAQ_ACCURACY_STATES[4] = {"Stabilizing", "Uncertain", "Calibrating", "Calibrated"};

static const uint8_t BSEC_CONFIG_IAQ_LP[] = {
#include "config/generic_33v_3s_28d/bsec_iaq.txt"
};
static const uint8_t BSEC_CONFIG_IAQ_ULP[] = {
#include "config/generic_33v_300s_28d/bsec_iaq.txt"
};

std::map<uint8_t, BME680BSECComponent *> BME680BSECComponent::instances;

void BME680BSECComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BME680 via BSEC...");
  BME680BSECComponent::instances[this->address_] = this;
  this->bsec_.begin(this->address_, BME680_I2C_INTF, BME680BSECComponent::read_bytes_wrapper,
                    BME680BSECComponent::write_bytes_wrapper, Bsec::delay_ms);

  if (!this->check_bsec_status_()) {
    this->mark_failed();
    return;
  }

  const uint8_t *bsec_config = BSEC_CONFIG_IAQ_LP;
  float bsec_sample_rate = BSEC_SAMPLE_RATE_LP;
  if (this->sample_rate_ == SAMPLE_RATE_ULP) {
    bsec_config = BSEC_CONFIG_IAQ_ULP;
    bsec_sample_rate = BSEC_SAMPLE_RATE_ULP;
  }

  this->bsec_.setConfig(bsec_config);
  this->load_state_();

  // Subscribe to sensor values
  bsec_virtual_sensor_t sensor_list[7] = {
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_GAS,
      this->iaq_mode_ == IAQ_MODE_STATIC ? BSEC_OUTPUT_STATIC_IAQ : BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
  };
  this->bsec_.updateSubscription(sensor_list, 7, bsec_sample_rate);
}

void BME680BSECComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "BME680 via BSEC:");
  ESP_LOGCONFIG(TAG, "  BSEC Version: %d.%d.%d.%d", this->bsec_.version.major, this->bsec_.version.minor,
                this->bsec_.version.major_bugfix, this->bsec_.version.minor_bugfix);
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication failed with BSEC Status: %d, BME680 Status: %d", this->last_bsec_status_,
             this->last_bme680_status_);
  }

  ESP_LOGCONFIG(TAG, "  Temperature Offset: %.2f", this->temperature_offset_);
  ESP_LOGCONFIG(TAG, "  IAQ Mode: %s", this->iaq_mode_ == IAQ_MODE_STATIC ? "Static" : "Mobile");
  ESP_LOGCONFIG(TAG, "  Sample Rate: %s", this->sample_rate_ == SAMPLE_RATE_ULP ? "ULP" : "LP");
  ESP_LOGCONFIG(TAG, "  State Save Interval: %ims", this->state_save_interval_);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Gas Resistance", this->gas_resistance_sensor_);
  LOG_SENSOR("  ", "IAQ", this->iaq_sensor_);
  LOG_SENSOR("  ", "Numeric IAQ Accuracy", this->iaq_accuracy_sensor_);
  LOG_TEXT_SENSOR("  ", "IAQ Accuracy", this->iaq_accuracy_text_sensor_);
  LOG_SENSOR("  ", "CO2 Equivalent", this->co2_equivalent_sensor_);
  LOG_SENSOR("  ", "Breath VOC Equivalent", this->breath_voc_equivalent_sensor_);
}

float BME680BSECComponent::get_setup_priority() const { return setup_priority::DATA; }

void BME680BSECComponent::loop() {
  if (this->check_bsec_status_() && this->bsec_.run()) {
    yield();
    this->save_state_();
    this->publish_state_(this->temperature_sensor_, this->bsec_.temperature);
    this->publish_state_(this->humidity_sensor_, this->bsec_.humidity);
    this->publish_state_(this->pressure_sensor_, this->bsec_.pressure / 100.0);
    this->publish_state_(this->gas_resistance_sensor_, this->bsec_.gasResistance);
    this->publish_state_(this->iaq_sensor_, this->get_iaq_());
    this->publish_state_(this->iaq_accuracy_text_sensor_, IAQ_ACCURACY_STATES[this->get_iaq_accuracy_()]);
    this->publish_state_(this->iaq_accuracy_sensor_, this->get_iaq_accuracy_(), true);
    this->publish_state_(this->co2_equivalent_sensor_, this->bsec_.co2Equivalent);
    this->publish_state_(this->breath_voc_equivalent_sensor_, this->bsec_.breathVocEquivalent);
  }
}

void BME680BSECComponent::publish_state_(sensor::Sensor *sensor, float value, bool change_only) {
  if (!sensor || (change_only && sensor->has_state() && sensor->state == value)) {
    return;
  }
  sensor->publish_state(value);
  yield();
}

void BME680BSECComponent::publish_state_(text_sensor::TextSensor *sensor, std::string value) {
  if (!sensor || (sensor->has_state() && sensor->state == value)) {
    return;
  }
  sensor->publish_state(value);
  yield();
}

void BME680BSECComponent::set_temperature_offset(float offset) {
  this->temperature_offset_ = offset;
  this->bsec_.setTemperatureOffset(offset);
}

void BME680BSECComponent::set_iaq_mode(IAQMode iaq_mode) { this->iaq_mode_ = iaq_mode; }

void BME680BSECComponent::set_sample_rate(SampleRate sample_rate) { this->sample_rate_ = sample_rate; }

void BME680BSECComponent::set_state_save_interval(uint32_t interval) { this->state_save_interval_ = interval; }

float BME680BSECComponent::get_iaq_() {
  return this->iaq_mode_ == IAQ_MODE_STATIC ? this->bsec_.staticIaq : this->bsec_.iaq;
}

uint8_t BME680BSECComponent::get_iaq_accuracy_() {
  return this->iaq_mode_ == IAQ_MODE_STATIC ? this->bsec_.staticIaqAccuracy : this->bsec_.iaqAccuracy;
}

int8_t BME680BSECComponent::read_bytes_wrapper(uint8_t address, uint8_t a_register, uint8_t *data, uint16_t len) {
  return BME680BSECComponent::instances[address]->read_bytes(a_register, data, len) ? 0 : -1;
}

int8_t BME680BSECComponent::write_bytes_wrapper(uint8_t address, uint8_t a_register, uint8_t *data, uint16_t len) {
  return BME680BSECComponent::instances[address]->write_bytes(a_register, data, len) ? 0 : -1;
}

bool BME680BSECComponent::check_bsec_status_() {
  if (this->bsec_.status != BSEC_OK && this->bsec_.status != this->last_bsec_status_) {
    if (this->bsec_.status < BSEC_OK) {
      ESP_LOGE(TAG, "BSEC error code: %d", this->bsec_.status);
      this->status_set_error();
    } else {
      ESP_LOGW(TAG, "BSEC warning code: %d", this->bsec_.status);
      this->status_set_warning();
    }
  }

  if (this->bsec_.bme680Status != BME680_OK && this->bsec_.bme680Status != this->last_bme680_status_) {
    if (this->bsec_.bme680Status < BME680_OK) {
      ESP_LOGE(TAG, "BME680 error code: %d", this->bsec_.bme680Status);
      this->status_set_error();
    } else {
      ESP_LOGW(TAG, "BME680 warning code: %d", this->bsec_.bme680Status);
      this->status_set_warning();
    }
  }

  this->last_bsec_status_ = this->bsec_.status;
  this->last_bme680_status_ = this->bsec_.bme680Status;

  bool ok = (this->last_bsec_status_ == BSEC_OK && this->last_bme680_status_ == BME680_OK);
  if (ok) {
    this->status_clear_error();
    this->status_clear_warning();
  }
  return ok;
}

void BME680BSECComponent::load_state_() {
  uint32_t hash = fnv1_hash("bme680_bsec_state_" + to_string(this->address_));
  this->bsec_state_ = global_preferences.make_preference<uint8_t[BSEC_MAX_STATE_BLOB_SIZE]>(hash, true);

  uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
  if (this->bsec_state_.load(&state)) {
    yield();
    ESP_LOGI(TAG, "Loading state");
    this->bsec_.setState(state);
    yield();
    this->check_bsec_status_();
  }
}

void BME680BSECComponent::save_state_() {
  static unsigned long last_millis = 0;
  if (this->get_iaq_accuracy_() < 3 || (millis() - last_millis < this->state_save_interval_)) {
    return;
  }

  uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
  this->bsec_.getState(state);
  yield();
  if (!this->check_bsec_status_()) {
    return;
  }

  ESP_LOGI(TAG, "Saving state");
  this->bsec_state_.save(&state);
  last_millis = millis();
  yield();
}

}  // namespace bme680_bsec
}  // namespace esphome

#endif

#include "zyaura.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zyaura {

static const char *const TAG = "zyaura";

bool IRAM_ATTR ZaDataProcessor::decode(uint32_t ms, bool data) {
  // check if a new message has started, based on time since previous bit
  if ((ms - this->prev_ms_) > ZA_MAX_MS) {
    this->num_bits_ = 0;
  }
  this->prev_ms_ = ms;

  // number of bits received is basically the "state"
  if (this->num_bits_ < ZA_FRAME_SIZE) {
    // store it while it fits
    int idx = this->num_bits_ / 8;
    this->buffer_[idx] = (this->buffer_[idx] << 1) | (data ? 1 : 0);
    this->num_bits_++;

    // are we done yet?
    if (this->num_bits_ == ZA_FRAME_SIZE) {
      // validate checksum
      uint8_t checksum = this->buffer_[ZA_BYTE_TYPE] + this->buffer_[ZA_BYTE_HIGH] + this->buffer_[ZA_BYTE_LOW];
      if (checksum != this->buffer_[ZA_BYTE_SUM] || this->buffer_[ZA_BYTE_END] != ZA_MSG_DELIMETER) {
        return false;
      }

      this->message->type = (ZaDataType) this->buffer_[ZA_BYTE_TYPE];
      this->message->value = this->buffer_[ZA_BYTE_HIGH] << 8 | this->buffer_[ZA_BYTE_LOW];
      return true;
    }
  }

  return false;
}

void ZaSensorStore::setup(InternalGPIOPin *pin_clock, InternalGPIOPin *pin_data) {
  pin_clock->setup();
  pin_data->setup();
  this->pin_clock_ = pin_clock->to_isr();
  this->pin_data_ = pin_data->to_isr();
  pin_clock->attach_interrupt(ZaSensorStore::interrupt, this, gpio::INTERRUPT_FALLING_EDGE);
}

void IRAM_ATTR ZaSensorStore::interrupt(ZaSensorStore *arg) {
  uint32_t now = millis();
  bool data_bit = arg->pin_data_.digital_read();

  if (arg->processor_.decode(now, data_bit)) {
    arg->set_data_(arg->processor_.message);
  }
}

void IRAM_ATTR ZaSensorStore::set_data_(ZaMessage *message) {
  switch (message->type) {
    case HUMIDITY:
      this->humidity = message->value;
      break;
    case TEMPERATURE:
      this->temperature = message->value;
      break;
    case CO2:
      this->co2 = message->value;
      break;
  }
}

bool ZyAuraSensor::publish_state_(ZaDataType data_type, sensor::Sensor *sensor, uint16_t *data_value) {
  // Sensor wasn't added to configuration
  if (sensor == nullptr) {
    return true;
  }

  float value = NAN;
  switch (data_type) {
    case HUMIDITY:
      value = (*data_value > 10000) ? NAN : (*data_value / 100.0f);
      break;
    case TEMPERATURE:
      value = (*data_value > 5970) ? NAN : (*data_value / 16.0f - 273.15f);
      break;
    case CO2:
      value = (*data_value > 10000) ? NAN : *data_value;
      break;
  }

  sensor->publish_state(value);

  // Sensor reported wrong value
  if (std::isnan(value)) {
    ESP_LOGW(TAG, "Sensor reported invalid data. Is the update interval too small?");
    this->status_set_warning();
    return false;
  }

  *data_value = -1;
  return true;
}

void ZyAuraSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ZyAuraSensor:");
  LOG_PIN("  Pin Clock: ", this->pin_clock_);
  LOG_PIN("  Pin Data: ", this->pin_data_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void ZyAuraSensor::update() {
  bool co2_result = this->publish_state_(CO2, this->co2_sensor_, &this->store_.co2);
  bool temperature_result = this->publish_state_(TEMPERATURE, this->temperature_sensor_, &this->store_.temperature);
  bool humidity_result = this->publish_state_(HUMIDITY, this->humidity_sensor_, &this->store_.humidity);

  if (co2_result && temperature_result && humidity_result) {
    this->status_clear_warning();
  }
}

}  // namespace zyaura
}  // namespace esphome

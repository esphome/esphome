#include "zyaura.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zyaura {

static const char *TAG = "zyaura";

ZaMessage* ICACHE_RAM_ATTR ZaDataProcessor::process(unsigned long ms, bool data) {
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
    // are we done yet?
    this->num_bits_++;
    if (this->num_bits_ == ZA_FRAME_SIZE) {
      this->decode_();
      return this->msg_;
    }
  }

  return nullptr;
}

void ICACHE_RAM_ATTR ZaDataProcessor::decode_() {
  uint8_t checksum = this->buffer_[ZA_BYTE_TYPE] + this->buffer_[ZA_BYTE_HIGH] + this->buffer_[ZA_BYTE_LOW];
  this->msg_->checksumIsValid = (checksum == this->buffer_[ZA_BYTE_SUM] && this->buffer_[ZA_BYTE_END] == ZA_MSG_DELIMETER);
  if (!this->msg_->checksumIsValid) {
    return;
  }

  this->msg_->type = (ZaDataType)this->buffer_[ZA_BYTE_TYPE];
  this->msg_->value = this->buffer_[ZA_BYTE_HIGH] << 8 | this->buffer_[ZA_BYTE_LOW];
}

void ZaSensorStore::setup(GPIOPin *pin_clock, GPIOPin *pin_data) {
  pin_clock->setup();
  pin_data->setup();
  this->pin_clock_ = pin_clock->to_isr();
  this->pin_data_ = pin_data->to_isr();
  pin_clock->attach_interrupt(ZaSensorStore::interrupt, this, FALLING);
}

void ICACHE_RAM_ATTR ZaSensorStore::interrupt(ZaSensorStore *arg) {
  uint32_t now = millis();
  bool dataBit = arg->pin_data_->digital_read();
  ZaMessage *message = arg->processor_.process(now, dataBit);

  if (message) {
    arg->set_data_(message);
  }
}

void ICACHE_RAM_ATTR ZaSensorStore::set_data_(ZaMessage *message) {
  if (!message->checksumIsValid) {
    this->isValid = false;
    return;
  }

  switch (message->type) {
    case HUMIDITY:
      this->humidity = (message->value > 10000) ? NAN : (message->value / 100.0f);
      break;

    case TEMPERATURE:
      this->temperature = (message->value > 5970) ? NAN : (message->value / 16.0f - 273.15f);
      break;

    case CO2:
      this->co2 = (message->value > 10000) ? NAN : message->value;
      break;

    default:
      break;
  }

  this->isValid = true;
}

bool ZyAuraSensor::publish_state_(sensor::Sensor *sensor, float value) {
  // Sensor doesn't added to configuration
  if (sensor == nullptr) {
    return true;
  }

  // Sensor reported wrong value
  if (isnan(value)) {
    ESP_LOGW(TAG, "Sensor reported invalid data. Is the update interval too small?");
    this->status_set_warning();
    return false;
  }

  sensor->publish_state(value);
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
  if (!this->store_.isValid) {
    ESP_LOGW(TAG, "Sensor reported data with invalid checksum. Please check pins configuration.");
    this->status_set_warning();
    return;
  }

  bool co2_result = this->publish_state_(this->co2_sensor_, this->store_.co2);
  bool temperature_result = this->publish_state_(this->temperature_sensor_, this->store_.temperature);
  bool humidity_result = this->publish_state_(this->humidity_sensor_, this->store_.humidity);

  if (co2_result && temperature_result && humidity_result) {
    this->status_clear_warning();
  }
}

}  // namespace zyaura
}  // namespace esphome

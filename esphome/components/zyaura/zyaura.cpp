#include "zyaura.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zyaura {

static const char *TAG = "zyaura";

ZaMessage* ZaDataProcessor::process(unsigned long ms, bool data) {
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

void ZaDataProcessor::decode_() {
  uint8_t checksum = this->buffer_[ZA_BYTE_TYPE] + this->buffer_[ZA_BYTE_HIGH] + this->buffer_[ZA_BYTE_LOW];
  this->msg_->checksumIsValid = (checksum == this->buffer_[ZA_BYTE_SUM] && this->buffer_[ZA_BYTE_END] == ZA_MSG_DELIMETER);
  if (!this->msg_->checksumIsValid) {
    return;
  }

  this->msg_->type = (ZaDataType)this->buffer_[ZA_BYTE_TYPE];
  this->msg_->value = this->buffer_[ZA_BYTE_HIGH] << ZA_BITS_IN_BYTE | this->buffer_[ZA_BYTE_LOW];
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
    if (!message->checksumIsValid) {
      ESP_LOGW(TAG, "Checksum validation error");
    } else if (!arg->set_value_(message)) {
      ESP_LOGW(TAG, "Sensor reported invalid data. Is the update interval too small?");
    }
  }
}

bool ZaSensorStore::set_value_(ZaMessage *message) {
  switch (message->type) {
    case HUMIDITY:
      if (message->value > 9999) {
        return false;
      }
      this->humidity = message->value / 100.0f;
      break;

    case TEMPERATURE:
      if (message->value > 5970) {
        return false;
      }
      this->temperature = message->value / 16.0f - 273.15f;
      break;

    case CO2:
      if (message->value > 9999) {
        return false;
      }
      this->co2 = message->value;
      break;

    default:
      break;
  }

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
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(this->store_.co2);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(this->store_.temperature);
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(this->store_.humidity);
}

}  // namespace zyaura
}  // namespace esphome

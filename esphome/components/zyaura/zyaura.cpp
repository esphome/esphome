#include "zyaura.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

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
    int idx = this->num_bits_ / ZA_BITS_IN_BYTE;
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
  this->msg_->inBoot = (this->msg_->type == CO2 && this->msg_->value > 10000);
}

void ZaSensorStore::setup(GPIOPin *pin_clock, GPIOPin *pin_data) {
  pin_clock->setup();
  pin_data->setup();
  this->pin_clock_ = pin_clock;
  this->pin_data_ = pin_data;
  pin_clock->attach_interrupt(ZaSensorStore::interrupt, this, FALLING);
}

void ZaSensorStore::interrupt(ZaSensorStore *arg) {
  unsigned long now = millis();
  bool dataBit = arg->pin_data_->digital_read();
  ZaMessage *message = arg->processor_.process(now, dataBit);

  if (message) {
    if (!message->checksumIsValid) {
      ESP_LOGW(TAG, "Checksum validation error");
    } else if (message->inBoot) {
      ESP_LOGW(TAG, "Sensor reported invalid data. Is the update interval too small?");
    } else {
      arg->process_(message);
    }
  }
}

void ZaSensorStore::process_(ZaMessage *message) {
  switch (message->type) {
    case HUMIDITY:
      this->humidity = (double)message->value / 100;
      break;

    case TEMPERATURE:
      this->temperature = (double)message->value / 16 - 273.15;
      break;

    case CO2:
      this->co2 = message->value;
      break;

    default:
      break;
  }
}

void ZyAura::dump_config() {
  ESP_LOGCONFIG(TAG, "ZyAura:");
  LOG_PIN("  Pin Clock: ", this->pin_clock_);
  LOG_PIN("  Pin Data: ", this->pin_data_);
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void ZyAura::update() {
  this->temperature_sensor_->publish_state(this->store_.temperature);
  this->co2_sensor_->publish_state(this->store_.co2);
  this->humidity_sensor_->publish_state(this->store_.humidity);
}

}  // namespace zyaura
}  // namespace esphome

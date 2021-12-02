#include "ezo.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ezo {

static const char *const TAG = "ezo.sensor";

static const uint16_t EZO_STATE_WAIT = 1;
static const uint16_t EZO_STATE_SEND_TEMP = 2;
static const uint16_t EZO_STATE_WAIT_TEMP = 4;

void EZOSensor::dump_config() {
  LOG_SENSOR("", "EZO", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
    ESP_LOGE(TAG, "Communication with EZO circuit failed!");
  LOG_UPDATE_INTERVAL(this);
}

void EZOSensor::update() {
  if (this->state_ & EZO_STATE_WAIT) {
    ESP_LOGE(TAG, "update overrun, still waiting for previous response");
    return;
  }
  uint8_t c = 'R';
  this->write(&c, 1);
  this->state_ |= EZO_STATE_WAIT;
  this->start_time_ = millis();
  this->wait_time_ = 900;
}

void EZOSensor::loop() {
  uint8_t buf[21];
  if (!(this->state_ & EZO_STATE_WAIT)) {
    if (this->state_ & EZO_STATE_SEND_TEMP) {
      int len = sprintf((char *) buf, "T,%0.3f", this->tempcomp_);
      this->write(buf, len);
      this->state_ = EZO_STATE_WAIT | EZO_STATE_WAIT_TEMP;
      this->start_time_ = millis();
      this->wait_time_ = 300;
    }
    return;
  }
  if (millis() - this->start_time_ < this->wait_time_)
    return;
  buf[0] = 0;
  if (!this->read_bytes_raw(buf, 20)) {
    ESP_LOGE(TAG, "read error");
    this->state_ = 0;
    return;
  }
  switch (buf[0]) {
    case 1:
      break;
    case 2:
      ESP_LOGE(TAG, "device returned a syntax error");
      break;
    case 254:
      return;  // keep waiting
    case 255:
      ESP_LOGE(TAG, "device returned no data");
      break;
    default:
      ESP_LOGE(TAG, "device returned an unknown response: %d", buf[0]);
      break;
  }
  if (this->state_ & EZO_STATE_WAIT_TEMP) {
    this->state_ = 0;
    return;
  }
  this->state_ &= ~EZO_STATE_WAIT;
  if (buf[0] != 1)
    return;

  // some sensors return multiple comma-separated values, terminate string after first one
  for (size_t i = 1; i < sizeof(buf) - 1; i++)
    if (buf[i] == ',')
      buf[i] = '\0';

  float val = parse_number<float>((char *) &buf[1], sizeof(buf) - 2).value_or(0);
  this->publish_state(val);
}

void EZOSensor::set_tempcomp_value(float temp) {
  this->tempcomp_ = temp;
  this->state_ |= EZO_STATE_SEND_TEMP;
}

}  // namespace ezo
}  // namespace esphome

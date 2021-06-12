#include "ezo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ezo {

static const char *TAG = "ezo.sensor";

static const uint16_t EZO_STATE_WAIT = 1;
static const uint16_t EZO_STATE_SEND_TEMP = 2;
static const uint16_t EZO_STATE_WAIT_TEMP = 4;
static const uint16_t EZO_STATE_SEND_PROBE_TYPE = 8;
static const uint16_t EZO_STATE_WAIT_PROBE_TYPE = 16;
static const uint16_t EZO_STATE_SEND_CALIBRATION = 32;
static const uint16_t EZO_STATE_WAIT_CALIBRATION = 64;

static const uint16_t CAL_SINGLE = 0;
static const uint16_t CAL_LOW = 1;
static const uint16_t CAL_MEDIUM = 2;
static const uint16_t CAL_HIGH = 3;

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
  this->write_bytes_raw(&c, 1);
  this->state_ |= EZO_STATE_WAIT;
  this->start_time_ = millis();
  this->wait_time_ = 900;
}

void EZOSensor::loop() {
  uint8_t buf[20];
  int len;
  if (!(this->state_ & EZO_STATE_WAIT)) {
    if (this->state_ & EZO_STATE_SEND_TEMP) {
      int len = sprintf((char *) buf, "T,%0.3f", this->tempcomp_);
      this->write_bytes_raw(buf, len);
      this->state_ = EZO_STATE_WAIT | EZO_STATE_WAIT_TEMP;
      this->start_time_ = millis();
      this->wait_time_ = 300;
    }
    if (this->state_ & EZO_STATE_SEND_PROBE_TYPE) {
      int len = sprintf((char *) buf, "K,%0.3f", this->probe_type_);
      this->write_bytes_raw(buf, len);
      this->state_ = EZO_STATE_WAIT | EZO_STATE_WAIT_PROBE_TYPE;
      this->start_time_ = millis();
      this->wait_time_ = 300;
    }
    if (this->state_ & EZO_STATE_SEND_CALIBRATION) {
      if (this->calibration_type_ == CAL_SINGLE) {
        len = sprintf((char *) buf, "Cal,%0.3f", this->calibration_value_);
      } else if (this->calibration_type_ == CAL_LOW) {
        len = sprintf((char *) buf, "Cal,low,%0.3f", this->calibration_value_);
      } else if (this->calibration_type_ == CAL_MEDIUM) {
        len = sprintf((char *) buf, "Cal,mid,%0.3f", this->calibration_value_);
      } else {
        len = sprintf((char *) buf, "Cal,high,%0.3f", this->calibration_value_);
      }
      this->write_bytes_raw(buf, len);
      this->state_ = EZO_STATE_WAIT | EZO_STATE_WAIT_CALIBRATION;
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

  float val = strtof((char *) &buf[1], nullptr);
  this->publish_state(val);
}

void EZOSensor::set_tempcomp_value(float temp) {
  this->tempcomp_ = temp;
  this->state_ |= EZO_STATE_SEND_TEMP;
}

void EZOSensor::set_probe_type(float probe_type) {
  this->probe_type_ = probe_type;
  this->state_ |= EZO_STATE_SEND_PROBE_TYPE;
}

void EZOSensor::set_calibration_single(float value) {
  this->calibration_value_ = value;
  this->calibration_type_ = CAL_SINGLE;
  this->state_ |= EZO_STATE_SEND_CALIBRATION;
}

void EZOSensor::set_calibration_point(int point, float value) {
  this->calibration_value_ = value;
  this->calibration_type_ = point;
  this->state_ |= EZO_STATE_SEND_CALIBRATION;
}

}  // namespace ezo
}  // namespace esphome

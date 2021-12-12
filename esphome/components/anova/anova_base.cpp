#include "anova_base.h"
#include <cstdio>
#include <cstring>

namespace esphome {
namespace anova {

float ftoc(float f) { return (f - 32.0) * (5.0f / 9.0f); }

float ctof(float c) { return (c * 9.0f / 5.0f) + 32.0; }

AnovaPacket *AnovaCodec::clean_packet_() {
  this->packet_.length = strlen((char *) this->packet_.data);
  this->packet_.data[this->packet_.length] = '\0';
  ESP_LOGV("anova", "SendPkt: %s\n", this->packet_.data);
  return &this->packet_;
}

AnovaPacket *AnovaCodec::get_read_device_status_request() {
  this->current_query_ = READ_DEVICE_STATUS;
  sprintf((char *) this->packet_.data, "%s", CMD_READ_DEVICE_STATUS);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_read_target_temp_request() {
  this->current_query_ = READ_TARGET_TEMPERATURE;
  sprintf((char *) this->packet_.data, "%s", CMD_READ_TARGET_TEMP);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_read_current_temp_request() {
  this->current_query_ = READ_CURRENT_TEMPERATURE;
  sprintf((char *) this->packet_.data, "%s", CMD_READ_CURRENT_TEMP);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_read_unit_request() {
  this->current_query_ = READ_UNIT;
  sprintf((char *) this->packet_.data, "%s", CMD_READ_UNIT);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_read_data_request() {
  this->current_query_ = READ_DATA;
  sprintf((char *) this->packet_.data, "%s", CMD_READ_DATA);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_set_target_temp_request(float temperature) {
  this->current_query_ = SET_TARGET_TEMPERATURE;
  if (this->fahrenheit_)
    temperature = ctof(temperature);
  sprintf((char *) this->packet_.data, CMD_SET_TARGET_TEMP, temperature);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_set_unit_request(char unit) {
  this->current_query_ = SET_UNIT;
  sprintf((char *) this->packet_.data, CMD_SET_TEMP_UNIT, unit);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_start_request() {
  this->current_query_ = START;
  sprintf((char *) this->packet_.data, CMD_START);
  return this->clean_packet_();
}

AnovaPacket *AnovaCodec::get_stop_request() {
  this->current_query_ = STOP;
  sprintf((char *) this->packet_.data, CMD_STOP);
  return this->clean_packet_();
}

void AnovaCodec::decode(const uint8_t *data, uint16_t length) {
  char buf[32];
  memset(buf, 0, sizeof(buf));
  strncpy(buf, (char *) data, std::min<uint16_t>(length, sizeof(buf) - 1));
  this->has_target_temp_ = this->has_current_temp_ = this->has_unit_ = this->has_running_ = false;
  switch (this->current_query_) {
    case READ_DEVICE_STATUS: {
      if (!strncmp(buf, "stopped", 7)) {
        this->has_running_ = true;
        this->running_ = false;
      }
      if (!strncmp(buf, "running", 7)) {
        this->has_running_ = true;
        this->running_ = true;
      }
      break;
    }
    case START: {
      if (!strncmp(buf, "start", 5)) {
        this->has_running_ = true;
        this->running_ = true;
      }
      break;
    }
    case STOP: {
      if (!strncmp(buf, "stop", 4)) {
        this->has_running_ = true;
        this->running_ = false;
      }
      break;
    }
    case READ_TARGET_TEMPERATURE:
    case SET_TARGET_TEMPERATURE: {
      this->target_temp_ = parse_number<float>(str_until(buf, '\r')).value_or(0.0f);
      if (this->fahrenheit_)
        this->target_temp_ = ftoc(this->target_temp_);
      this->has_target_temp_ = true;
      break;
    }
    case READ_CURRENT_TEMPERATURE: {
      this->current_temp_ = parse_number<float>(str_until(buf, '\r')).value_or(0.0f);
      if (this->fahrenheit_)
        this->current_temp_ = ftoc(this->current_temp_);
      this->has_current_temp_ = true;
      break;
    }
    case SET_UNIT:
    case READ_UNIT: {
      this->unit_ = buf[0];
      this->fahrenheit_ = buf[0] == 'f';
      this->has_unit_ = true;
      break;
    }
    default:
      break;
  }
}

}  // namespace anova
}  // namespace esphome

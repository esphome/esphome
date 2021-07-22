#include "anova_base.h"

namespace esphome {
namespace anova {

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
  memset(this->buf_, 0, 32);
  strncpy(this->buf_, (char *) data, length);
  ESP_LOGV("anova", "Received: %s\n", this->buf_);
  this->has_target_temp_ = this->has_current_temp_ = this->has_unit_ = this->has_running_ = false;
  switch (this->current_query_) {
    case READ_DEVICE_STATUS: {
      if (!strncmp(this->buf_, "stopped", 7)) {
        this->has_running_ = true;
        this->running_ = false;
      }
      if (!strncmp(this->buf_, "running", 7)) {
        this->has_running_ = true;
        this->running_ = true;
      }
      break;
    }
    case START: {
      if (!strncmp(this->buf_, "start", 5)) {
        this->has_running_ = true;
        this->running_ = true;
      }
      break;
    }
    case STOP: {
      if (!strncmp(this->buf_, "stop", 4)) {
        this->has_running_ = true;
        this->running_ = false;
      }
      break;
    }
    case READ_TARGET_TEMPERATURE: {
      this->target_temp_ = strtof(this->buf_, nullptr);
      this->has_target_temp_ = true;
      break;
    }
    case SET_TARGET_TEMPERATURE: {
      this->target_temp_ = strtof(this->buf_, nullptr);
      this->has_target_temp_ = true;
      break;
    }
    case READ_CURRENT_TEMPERATURE: {
      this->current_temp_ = strtof(this->buf_, nullptr);
      this->has_current_temp_ = true;
      break;
    }
    default:
      break;
  }
}

}  // namespace anova
}  // namespace esphome

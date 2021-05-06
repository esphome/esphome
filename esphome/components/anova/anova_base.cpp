#include "anova_base.h"

namespace esphome {
namespace anova {

anova_packet *AnovaCodec::clean_packet() {
  this->packet_.length = strlen((char *) this->packet_.data);
  this->packet_.data[this->packet_.length] = '\0';
  ESP_LOGI("anova", "SendPkt: %s\n", this->packet_.data);
  return &this->packet_;
}

anova_packet *AnovaCodec::get_read_device_status_request() {
  this->current_query_ = ReadDeviceStatus;
  sprintf((char *) this->packet_.data, "%s", READ_DEVICE_STATUS);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_read_target_temp_request() {
  this->current_query_ = ReadTargetTemperature;
  sprintf((char *) this->packet_.data, "%s", READ_TARGET_TEMP);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_read_current_temp_request() {
  this->current_query_ = ReadCurrentTemperature;
  sprintf((char *) this->packet_.data, "%s", READ_CURRENT_TEMP);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_read_unit_request() {
  this->current_query_ = ReadUnit;
  sprintf((char *) this->packet_.data, "%s", READ_UNIT);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_read_data_request() {
  this->current_query_ = ReadData;
  sprintf((char *) this->packet_.data, "%s", READ_DATA);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_set_target_temp_request(float temperature) {
  this->current_query_ = SetTargetTemperature;
  sprintf((char *) this->packet_.data, SET_TARGET_TEMP, temperature);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_set_unit_request(char unit) {
  this->current_query_ = SetUnit;
  sprintf((char *) this->packet_.data, SET_TEMP_UNIT, unit);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_start_request() {
  this->current_query_ = Start;
  sprintf((char *) this->packet_.data, CMD_START);
  return this->clean_packet();
}

anova_packet *AnovaCodec::get_stop_request() {
  this->current_query_ = Stop;
  sprintf((char *) this->packet_.data, CMD_STOP);
  return this->clean_packet();
}

void AnovaCodec::decode(const uint8_t *data, uint16_t length) {
  memset(this->buf_, 0, 32);
  strncpy(this->buf_, (char *) data, length);
  ESP_LOGI("anova", "Received: %s\n", this->buf_);
  this->has_target_temp_ = this->has_current_temp_ = this->has_unit_ = this->has_running_ = false;
  switch (this->current_query_) {
    case ReadDeviceStatus: {
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
    case Start: {
      if (!strncmp(this->buf_, "start", 5)) {
        this->has_running_ = true;
        this->running_ = true;
      }
      break;
    }
    case Stop: {
      if (!strncmp(this->buf_, "stop", 4)) {
        this->has_running_ = true;
        this->running_ = false;
      }
      break;
    }
    case ReadTargetTemperature: {
      sscanf(this->buf_, "%f", &this->target_temp_);
      this->has_target_temp_ = true;
      break;
    }
    case SetTargetTemperature: {
      sscanf(this->buf_, "%f", &this->target_temp_);
      this->has_target_temp_ = true;
      break;
    }
    case ReadCurrentTemperature: {
      sscanf(this->buf_, "%f", &this->current_temp_);
      this->has_current_temp_ = true;
      break;
    }
    default:
      break;
  }
}

}  // namespace anova
}  // namespace esphome

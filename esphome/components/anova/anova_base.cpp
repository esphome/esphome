#include "anova_base.h"
#include <cstring>

#include "esphome/core/alloc_helpers.h"

namespace esphome::anova {

static constexpr char CMD_READ_DEVICE_STATUS[] = "status\r";
static constexpr char CMD_READ_TARGET_TEMP[] = "read set temp\r";
static constexpr char CMD_READ_CURRENT_TEMP[] = "read temp\r";
static constexpr char CMD_READ_UNIT[] = "read unit\r";
static constexpr char CMD_READ_DATA[] = "read data\r";
static constexpr char CMD_SET_TARGET_TEMP[] = "set temp ";
static constexpr char CMD_SET_TEMP_UNIT[] = "set unit ";
static constexpr char CMD_START[] = "start\r";
static constexpr char CMD_STOP[] = "stop\r";

float ftoc(float f) { return (f - 32.0f) * (5.0f / 9.0f); }

float ctof(float c) { return (c * 9.0f / 5.0f) + 32.0f; }

AnovaPacket *AnovaCodec::finish_packet_(size_t length) {
  this->packet_.length = length;
  ESP_LOGV("anova", "SendPkt: %s\n", this->packet_.data);
  return &this->packet_;
}

AnovaPacket *AnovaCodec::set_command_with_arg_(const char *prefix, const char *arg) {
  char *buf = reinterpret_cast<char *>(this->packet_.data);
  size_t pos = buf_append_str(buf, sizeof(this->packet_.data), 0, prefix);
  pos = buf_append_str(buf, sizeof(this->packet_.data), pos, arg);
  pos = buf_append_str(buf, sizeof(this->packet_.data), pos, "\r");
  return this->finish_packet_(pos);
}

AnovaPacket *AnovaCodec::get_read_device_status_request() {
  this->current_query_ = READ_DEVICE_STATUS;
  return this->set_command_(CMD_READ_DEVICE_STATUS);
}

AnovaPacket *AnovaCodec::get_read_target_temp_request() {
  this->current_query_ = READ_TARGET_TEMPERATURE;
  return this->set_command_(CMD_READ_TARGET_TEMP);
}

AnovaPacket *AnovaCodec::get_read_current_temp_request() {
  this->current_query_ = READ_CURRENT_TEMPERATURE;
  return this->set_command_(CMD_READ_CURRENT_TEMP);
}

AnovaPacket *AnovaCodec::get_read_unit_request() {
  this->current_query_ = READ_UNIT;
  return this->set_command_(CMD_READ_UNIT);
}

AnovaPacket *AnovaCodec::get_read_data_request() {
  this->current_query_ = READ_DATA;
  return this->set_command_(CMD_READ_DATA);
}

AnovaPacket *AnovaCodec::get_set_target_temp_request(float temperature) {
  this->current_query_ = SET_TARGET_TEMPERATURE;
  if (this->fahrenheit_)
    temperature = ctof(temperature);
  char value[VALUE_ACCURACY_MAX_LEN];
  value_accuracy_to_buf(value, temperature, 1);
  return this->set_command_with_arg_(CMD_SET_TARGET_TEMP, value);
}

AnovaPacket *AnovaCodec::get_set_unit_request(char unit) {
  this->current_query_ = SET_UNIT;
  const char arg[] = {unit, '\0'};
  return this->set_command_with_arg_(CMD_SET_TEMP_UNIT, arg);
}

AnovaPacket *AnovaCodec::get_start_request() {
  this->current_query_ = START;
  return this->set_command_(CMD_START);
}

AnovaPacket *AnovaCodec::get_stop_request() {
  this->current_query_ = STOP;
  return this->set_command_(CMD_STOP);
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
      this->target_temp_ = parse_number<float>(str_until(buf, '\r')).value_or(0.0f);  // NOLINT
      if (this->fahrenheit_)
        this->target_temp_ = ftoc(this->target_temp_);
      this->has_target_temp_ = true;
      break;
    }
    case READ_CURRENT_TEMPERATURE: {
      this->current_temp_ = parse_number<float>(str_until(buf, '\r')).value_or(0.0f);  // NOLINT
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

}  // namespace esphome::anova

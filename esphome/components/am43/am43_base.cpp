#include "am43_base.h"
#include <cstring>
#include <cstdio>

namespace esphome {
namespace am43 {

const uint8_t START_PACKET[5] = {0x00, 0xff, 0x00, 0x00, 0x9a};

std::string pkt_to_hex(const uint8_t *data, uint16_t len) {
  char buf[64];
  memset(buf, 0, 64);
  for (int i = 0; i < len; i++)
    sprintf(&buf[i * 2], "%02x", data[i]);
  std::string ret = buf;
  return ret;
}

Am43Packet *Am43Encoder::get_battery_level_request() {
  uint8_t data = 0x1;
  return this->encode_(0xA2, &data, 1);
}

Am43Packet *Am43Encoder::get_light_level_request() {
  uint8_t data = 0x1;
  return this->encode_(0xAA, &data, 1);
}

Am43Packet *Am43Encoder::get_position_request() {
  uint8_t data = 0x1;
  return this->encode_(CMD_GET_POSITION, &data, 1);
}

Am43Packet *Am43Encoder::get_send_pin_request(uint16_t pin) {
  uint8_t data[2];
  data[0] = (pin & 0xFF00) >> 8;
  data[1] = pin & 0xFF;
  return this->encode_(CMD_SEND_PIN, data, 2);
}

Am43Packet *Am43Encoder::get_open_request() {
  uint8_t data = 0xDD;
  return this->encode_(CMD_SET_STATE, &data, 1);
}

Am43Packet *Am43Encoder::get_close_request() {
  uint8_t data = 0xEE;
  return this->encode_(CMD_SET_STATE, &data, 1);
}

Am43Packet *Am43Encoder::get_stop_request() {
  uint8_t data = 0xCC;
  return this->encode_(CMD_SET_STATE, &data, 1);
}

Am43Packet *Am43Encoder::get_set_position_request(uint8_t position) {
  return this->encode_(CMD_SET_POSITION, &position, 1);
}

void Am43Encoder::checksum_() {
  uint8_t checksum = 0;
  int i = 0;
  for (i = 0; i < this->packet_.length; i++)
    checksum = checksum ^ this->packet_.data[i];
  this->packet_.data[i] = checksum ^ 0xff;
  this->packet_.length++;
}

Am43Packet *Am43Encoder::encode_(uint8_t command, uint8_t *data, uint8_t length) {
  memcpy(this->packet_.data, START_PACKET, 5);
  this->packet_.data[5] = command;
  this->packet_.data[6] = length;
  memcpy(&this->packet_.data[7], data, length);
  this->packet_.length = length + 7;
  this->checksum_();
  ESP_LOGV("am43", "ENC(%d): 0x%s", packet_.length, pkt_to_hex(packet_.data, packet_.length).c_str());
  return &this->packet_;
}

#define VERIFY_MIN_LENGTH(x) \
  if (length < (x)) \
    return;

void Am43Decoder::decode(const uint8_t *data, uint16_t length) {
  this->has_battery_level_ = false;
  this->has_light_level_ = false;
  this->has_set_position_response_ = false;
  this->has_set_state_response_ = false;
  this->has_position_ = false;
  this->has_pin_response_ = false;
  ESP_LOGV("am43", "DEC(%d): 0x%s", length, pkt_to_hex(data, length).c_str());

  if (length < 2 || data[0] != 0x9a)
    return;
  switch (data[1]) {
    case CMD_GET_BATTERY_LEVEL: {
      VERIFY_MIN_LENGTH(8);
      this->battery_level_ = data[7];
      this->has_battery_level_ = true;
      break;
    }
    case CMD_GET_LIGHT_LEVEL: {
      VERIFY_MIN_LENGTH(5);
      this->light_level_ = 100 * ((float) data[4] / 9);
      this->has_light_level_ = true;
      break;
    }
    case CMD_GET_POSITION: {
      VERIFY_MIN_LENGTH(6);
      this->position_ = data[5];
      this->has_position_ = true;
      break;
    }
    case CMD_NOTIFY_POSITION: {
      VERIFY_MIN_LENGTH(5);
      this->position_ = data[4];
      this->has_position_ = true;
      break;
    }
    case CMD_SEND_PIN: {
      VERIFY_MIN_LENGTH(4);
      this->pin_ok_ = data[3] == RESPONSE_ACK;
      this->has_pin_response_ = true;
      break;
    }
    case CMD_SET_POSITION: {
      VERIFY_MIN_LENGTH(4);
      this->set_position_ok_ = data[3] == RESPONSE_ACK;
      this->has_set_position_response_ = true;
      break;
    }
    case CMD_SET_STATE: {
      VERIFY_MIN_LENGTH(4);
      this->set_state_ok_ = data[3] == RESPONSE_ACK;
      this->has_set_state_response_ = true;
      break;
    }
    default:
      break;
  }
};

}  // namespace am43
}  // namespace esphome

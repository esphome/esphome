#include "am43_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::am43 {

ESPHOME_LOG_TAG(TAG, "am43");

static constexpr uint8_t START_PACKET[5] = {0x00, 0xff, 0x00, 0x00, 0x9a};

Am43Packet Am43Encoder::get_battery_level_request() {
  uint8_t data = 0x1;
  return encode(0xA2, &data, 1);
}

Am43Packet Am43Encoder::get_light_level_request() {
  uint8_t data = 0x1;
  return encode(0xAA, &data, 1);
}

Am43Packet Am43Encoder::get_position_request() {
  uint8_t data = 0x1;
  return encode(CMD_GET_POSITION, &data, 1);
}

Am43Packet Am43Encoder::get_send_pin_request(uint16_t pin) {
  uint8_t data[2];
  data[0] = (pin & 0xFF00) >> 8;
  data[1] = pin & 0xFF;
  return encode(CMD_SEND_PIN, data, 2);
}

Am43Packet Am43Encoder::get_open_request() {
  uint8_t data = 0xDD;
  return encode(CMD_SET_STATE, &data, 1);
}

Am43Packet Am43Encoder::get_close_request() {
  uint8_t data = 0xEE;
  return encode(CMD_SET_STATE, &data, 1);
}

Am43Packet Am43Encoder::get_stop_request() {
  uint8_t data = 0xCC;
  return encode(CMD_SET_STATE, &data, 1);
}

Am43Packet Am43Encoder::get_set_position_request(uint8_t position) { return encode(CMD_SET_POSITION, &position, 1); }

Am43Packet Am43Encoder::encode(uint8_t command, const uint8_t *data, uint8_t length) {
  Am43Packet packet;
  memcpy(packet.data, START_PACKET, sizeof(START_PACKET));
  packet.data[5] = command;
  packet.data[6] = length;
  memcpy(&packet.data[7], data, length);
  packet.length = length + 7;
  // The device checks the inverted XOR of everything before it
  packet.data[packet.length] = xor_bytes(packet.data, packet.length) ^ 0xff;
  packet.length++;
  char hex_buf[format_hex_size(sizeof(packet.data))];
  ESP_LOGV(TAG, "ENC(%d): 0x%s", packet.length, format_hex_to(hex_buf, packet.data, packet.length));
  return packet;
}

void Am43Decoder::decode(const uint8_t *data, uint16_t length) {
  this->has_battery_level_ = false;
  this->has_light_level_ = false;
  this->has_set_position_response_ = false;
  this->has_set_state_response_ = false;
  this->has_position_ = false;
  this->has_pin_response_ = false;
  char hex_buf[format_hex_size(sizeof(Am43Packet::data))];  // Max expected packet size
  ESP_LOGV(TAG, "DEC(%d): 0x%s", length, format_hex_to(hex_buf, data, length));

  if (length < 2 || data[0] != 0x9a)
    return;
  switch (data[1]) {
    case CMD_GET_BATTERY_LEVEL: {
      if (length < 8)
        return;
      this->battery_level_ = data[7];
      this->has_battery_level_ = true;
      break;
    }
    case CMD_GET_LIGHT_LEVEL: {
      if (length < 5)
        return;
      this->light_level_ = 100 * ((float) data[4] / 9);
      this->has_light_level_ = true;
      break;
    }
    case CMD_GET_POSITION: {
      if (length < 6)
        return;
      this->position_ = data[5];
      this->has_position_ = true;
      break;
    }
    case CMD_NOTIFY_POSITION: {
      if (length < 5)
        return;
      this->position_ = data[4];
      this->has_position_ = true;
      break;
    }
    case CMD_SEND_PIN: {
      if (length < 4)
        return;
      this->pin_ok_ = data[3] == RESPONSE_ACK;
      this->has_pin_response_ = true;
      break;
    }
    case CMD_SET_POSITION: {
      if (length < 4)
        return;
      this->set_position_ok_ = data[3] == RESPONSE_ACK;
      this->has_set_position_response_ = true;
      break;
    }
    case CMD_SET_STATE: {
      if (length < 4)
        return;
      this->set_state_ok_ = data[3] == RESPONSE_ACK;
      this->has_set_state_response_ = true;
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::am43

#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace am43_cover {

struct am43_packet {
  uint8_t length;
  uint8_t data[24];
};

#define CMD_GET_BATTERY_LEVEL 0xA2
#define CMD_GET_LIGHT_LEVEL 0xAA
#define CMD_GET_POSITION 0xA7
#define CMD_SEND_PIN 0x17
#define CMD_SET_STATE 0x0A
#define CMD_SET_POSITION 0x0D
#define CMD_NOTIFY_POSITION 0xA1

#define RESPONSE_ACK 0x5a
#define RESPONSE_NACK 0xA5

class Am43Encoder {
 public:
  am43_packet* get_battery_level_request();
  am43_packet* get_light_level_request();
  am43_packet* get_position_request();
  am43_packet* get_send_pin_request(uint16_t pin);
  am43_packet* get_open_request();
  am43_packet* get_close_request();
  am43_packet* get_stop_request();
  am43_packet* get_set_position_request(uint8_t position);

 protected:
  void checksum();
  am43_packet* encode(uint8_t command, uint8_t *data, uint8_t length);
  am43_packet packet;
};

class Am43Decoder {
 public:
  void decode(const uint8_t *data, uint16_t length);
  bool has_battery_level() { return this->has_battery_level_;}
  bool has_light_level() { return this->has_light_level_; }
  bool has_set_position_response() { return this->has_set_position_response_; }
  bool has_set_state_response() { return this->has_set_state_response_; }
  bool has_position() { return this->has_position_; }
  bool has_pin_response() { return this->has_pin_response_; }

  union {
    uint8_t position_;
    uint8_t battery_level_;
    float light_level_;
    uint8_t set_position_ok_;
    uint8_t set_state_ok_;
    uint8_t pin_ok_;
  };

 protected:
  bool has_battery_level_;
  bool has_light_level_;
  bool has_set_position_response_;
  bool has_set_state_response_;
  bool has_position_;
  bool has_pin_response_;
};

}
}

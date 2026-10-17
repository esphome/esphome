#pragma once

#include <cstddef>
#include <cstdint>

// BTHome v2 service data parser. No ESPHome headers.

namespace esphome::bthome::codec {

static constexpr uint8_t OBJECT_PACKET_ID = 0x00;
static constexpr uint8_t OBJECT_BUTTON = 0x3A;
static constexpr uint8_t MAX_BUTTONS = 8;
static constexpr size_t MAC_LEN = 6;

struct Parsed {
  bool encrypted;
  bool has_mac;
  bool has_packet_id;
  uint8_t packet_id;
  uint8_t button_count;
  // As sent: least significant byte first.
  uint8_t mac[MAC_LEN];
  uint8_t buttons[MAX_BUTTONS];
};

// `data` starts at the device-info byte (UUID already removed by the BLE stack).
// False for anything but an unencrypted v2 frame.
bool parse(const uint8_t *data, size_t len, Parsed *out);

}  // namespace esphome::bthome::codec

#pragma once

#include <cstddef>
#include <cstdint>

// BTHome v2 button codec. No ESPHome headers, so the host test can compile it.

namespace esphome {
namespace bthome {
namespace codec {

static constexpr uint8_t DEVICE_INFO_V2_TRIGGER = 0x44;
static constexpr uint8_t OBJECT_PACKET_ID = 0x00;
static constexpr uint8_t OBJECT_BUTTON = 0x3A;
static constexpr uint8_t MAX_BUTTONS = 8;

struct Parsed {
  bool ok;
  bool encrypted;
  bool mac_included;
  bool trigger_based;
  bool has_packet_id;
  uint8_t packet_id;
  uint8_t button_count;
  uint8_t buttons[MAX_BUTTONS];
};

// Writes device-info + objects (no 16-bit UUID). index is 1-based.
// Buttons before index are emitted as 0x00 (none), so object order stays stable.
bool encode_button(uint8_t packet_id, uint8_t event, uint8_t index, uint8_t *out, size_t cap, size_t *out_len);

// `data` starts at the device-info byte (UUID already removed by the BLE stack).
bool parse(const uint8_t *data, size_t len, Parsed *out);

}  // namespace codec
}  // namespace bthome
}  // namespace esphome

#include "codec.h"

namespace esphome::bthome::codec {

constexpr uint8_t INFO_ENCRYPTED = 0x01;
constexpr uint8_t INFO_MAC_INCLUDED = 0x02;
constexpr uint8_t INFO_TRIGGER = 0x04;
constexpr uint8_t INFO_VERSION_SHIFT = 5;
constexpr uint8_t INFO_VERSION_MASK = 0x07;
constexpr uint8_t VERSION_2 = 2;
constexpr uint8_t OBJECT_COMMAND = 0x3B;
constexpr uint8_t OBJECT_TEXT = 0x53;
constexpr uint8_t OBJECT_RAW = 0x54;
constexpr uint8_t BUTTON_NONE = 0x00;
constexpr uint8_t COMMAND_LENGTH_MASK = 0x1F;
constexpr size_t COMMAND_HEADER_LEN = 2;
constexpr size_t DEVICE_INFO_LEN = 1;
constexpr size_t LENGTH_PREFIX = 1;
constexpr size_t MAC_LEN = 6;
constexpr size_t OBJECT_ID_AND_VALUE = 2;

namespace {

// Sizes from the BTHome v2 object table. Skip only. These objects are not decoded.
int fixed_object_len(uint8_t id) {
  switch (id) {
    case 0x00:
    case 0x01:
    case 0x09:
    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x3A:
    case 0x46:
    case 0x57:
    case 0x58:
    case 0x59:
    case 0x60:
    case 0x64:
    case 0x65:
      return 1;
    case 0x02:
    case 0x03:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x3C:
    case 0x3D:
    case 0x3F:
    case 0x40:
    case 0x41:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x51:
    case 0x52:
    case 0x56:
    case 0x5A:
    case 0x5D:
    case 0x5E:
    case 0x5F:
    case 0x61:
    case 0xF0:
      return 2;
    case 0x04:
    case 0x05:
    case 0x0A:
    case 0x0B:
    case 0x42:
    case 0x4B:
    case 0xF2:
      return 3;
    case 0x3E:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
    case 0x50:
    case 0x55:
    case 0x5B:
    case 0x5C:
    case 0x62:
    case 0x63:
    case 0xF1:
      return 4;
    default:
      return -1;
  }
}

int object_payload_len(uint8_t id, const uint8_t *p, size_t avail) {
  // Variable length. Not decoded. Skipping it keeps a button that follows.
  if (id == OBJECT_COMMAND) {
    if (avail < COMMAND_HEADER_LEN) {
      return -1;
    }
    const int n = static_cast<int>(COMMAND_HEADER_LEN + (p[0] & COMMAND_LENGTH_MASK));
    return (avail < static_cast<size_t>(n)) ? -1 : n;
  }
  if (id == OBJECT_TEXT || id == OBJECT_RAW) {
    // The length byte counts. A declared length of 0 is still one byte,
    // otherwise a following button would be dropped with the rest of the packet.
    if (avail < LENGTH_PREFIX) {
      return -1;
    }
    const int n = static_cast<int>(LENGTH_PREFIX + p[0]);
    if (avail < static_cast<size_t>(n)) {
      return -1;
    }
    return n;
  }
  const int n = fixed_object_len(id);
  if (n < 0 || avail < static_cast<size_t>(n)) {
    return -1;
  }
  return n;
}

}  // namespace

bool encode_button(uint8_t packet_id, uint8_t event, uint8_t index, uint8_t *out, size_t cap, size_t *out_len) {
  if (out == nullptr || out_len == nullptr || index < 1 || index > MAX_BUTTONS) {
    return false;
  }
  const size_t need = DEVICE_INFO_LEN + OBJECT_ID_AND_VALUE + static_cast<size_t>(index) * OBJECT_ID_AND_VALUE;
  if (cap < need) {
    return false;
  }
  size_t i = 0;
  out[i++] = DEVICE_INFO_V2_TRIGGER;
  out[i++] = OBJECT_PACKET_ID;
  out[i++] = packet_id;
  for (uint8_t button = 1; button <= index; button++) {
    out[i++] = OBJECT_BUTTON;
    out[i++] = (button == index) ? event : BUTTON_NONE;
  }
  *out_len = i;
  return true;
}

bool parse(const uint8_t *data, size_t len, Parsed *out) {
  if (out == nullptr) {
    return false;
  }
  *out = Parsed{};
  if (data == nullptr || len < DEVICE_INFO_LEN) {
    return false;
  }
  const uint8_t info = data[0];
  if (((info >> INFO_VERSION_SHIFT) & INFO_VERSION_MASK) != VERSION_2) {
    return false;
  }
  out->encrypted = (info & INFO_ENCRYPTED) != 0;
  out->mac_included = (info & INFO_MAC_INCLUDED) != 0;
  out->trigger_based = (info & INFO_TRIGGER) != 0;
  if (out->encrypted) {
    return false;
  }
  size_t offset = DEVICE_INFO_LEN;
  if (out->mac_included) {
    if (len < DEVICE_INFO_LEN + MAC_LEN) {
      return false;
    }
    offset = DEVICE_INFO_LEN + MAC_LEN;
  }
  while (offset < len) {
    const uint8_t id = data[offset++];
    const int plen = object_payload_len(id, data + offset, len - offset);
    if (plen < 0) {
      break;
    }
    if (id == OBJECT_PACKET_ID) {
      out->has_packet_id = true;
      out->packet_id = data[offset];
    } else if (id == OBJECT_BUTTON) {
      if (out->button_count < MAX_BUTTONS) {
        out->buttons[out->button_count++] = data[offset];
      }
    }
    offset += static_cast<size_t>(plen);
  }
  out->ok = true;
  return true;
}

}  // namespace esphome::bthome::codec

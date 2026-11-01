#include "toptronic.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::toptronic {

ESPHOME_LOG_TAG(TAG, "toptronic");

static constexpr uint8_t RESPONSE = 0x42;
static constexpr uint8_t RESPONSE_EXT = 0x56;  // same layout + 2 header bytes before value
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;
static constexpr size_t MIN_MESSAGE_LEN = 5;  // cmd | fg | fn | dp_hi | dp_lo
static constexpr size_t CRC_LEN = 2;

static constexpr uint16_t sender_id(uint32_t can_id) { return (can_id >> 11) & 0x7FF; }

TopTronic::TopTronic(canbus::Canbus *canbus, uint16_t device_id) : device_id_(device_id) {
  canbus->add_callback([this](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
    // A remote frame has no payload; the bytes passed with it are left over from an earlier frame
    if (rtr || !extended_id || sender_id(can_id) != this->device_id_)
      return;
    const uint8_t *msg;
    size_t len;
    if (this->parse_frame_(data.data(), data.size(), can_id, &msg, &len))
      this->interpret_message_(msg, len);
  });
}

void TopTronic::dump_config() { ESP_LOGCONFIG(TAG, "TopTronic device 0x%04X", (unsigned) this->device_id_); }

bool TopTronic::parse_frame_(const uint8_t *data, size_t size, uint32_t can_id, const uint8_t **msg, size_t *len) {
  if (size < 2)
    return false;
  const uint32_t now = App.get_loop_component_start_time();
  const bool start = (can_id >> 24) == START_OF_MESSAGE_ID;
  // Start frame: [0] total frame count << 3, [1] tag, payload. Continuation: [0] tag, payload.
  const uint8_t tag = start ? data[1] : data[0];
  Slot *slot = nullptr;
  for (auto &s : this->slots_) {
    if (s.tag == tag && s.is_live(now)) {
      slot = &s;
      break;
    }
  }

  if (start) {
    const uint8_t total_frames = data[0] >> 3;
    if (total_frames == 0) {
      *msg = data + 1;
      *len = size - 1;
      return true;
    }
    if (total_frames == 1) {
      ESP_LOGV(TAG, "Dropping start frame with frame count 1");
      return false;
    }
    if (slot == nullptr) {
      // Take a free or stale slot, else evict the oldest
      slot = &this->slots_[0];
      for (auto &s : this->slots_) {
        if (!s.is_live(now)) {
          slot = &s;
          break;
        }
        if (now - s.last_update_ms > now - slot->last_update_ms)
          slot = &s;
      }
    }
    slot->tag = tag;
    slot->len = 0;
    slot->remaining = total_frames;  // the start frame is consumed below like a continuation
  } else if (slot == nullptr) {
    return false;
  }

  const size_t payload = size - (start ? 2 : 1);
  if (slot->len + payload > MAX_MESSAGE_LEN) {
    slot->remaining = 0;
    return false;
  }
  std::memcpy(slot->data + slot->len, data + (size - payload), payload);
  slot->len += payload;
  slot->last_update_ms = now;
  if (--slot->remaining != 0)
    return false;

  const uint8_t *buf = slot->data;
  const size_t buf_len = slot->len;
  if (buf_len < MIN_MESSAGE_LEN + CRC_LEN)
    return false;
  if (encode_uint16(buf[buf_len - 2], buf[buf_len - 1]) != crc16(buf, buf_len - CRC_LEN, 0x600D, 0x8408)) {
    ESP_LOGW(TAG, "CRC check failed");
    return false;
  }
  *msg = buf;
  *len = buf_len - CRC_LEN;
  return true;
}

// [0] cmd (0x40 GET, 0x46 SET, 0x42/0x56 RESPONSE), [1] fg, [2] fn,
// [3..4] datapoint, [5..] value (0x56 value starts at [7]).
void TopTronic::interpret_message_(const uint8_t *data, size_t len) {
  if (len == 0 || (data[0] != RESPONSE && data[0] != RESPONSE_EXT))
    return;
  const size_t value_off = data[0] == RESPONSE_EXT ? 7 : MIN_MESSAGE_LEN;
  if (len < value_off)
    return;
  ESP_LOGV(TAG, "Response 0x%02X from 0x%03X: fg=%u fn=%u datapoint=%u, %zu value bytes", data[0],
           (unsigned) this->device_id_, (unsigned) data[1], (unsigned) data[2],
           (unsigned) encode_uint16(data[3], data[4]), len - value_off);
}

}  // namespace esphome::toptronic

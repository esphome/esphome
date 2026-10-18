#include "toptronic.h"
#include "esphome/core/log.h"
namespace esphome::toptronic {
ESPHOME_LOG_TAG(TAG, "toptronic");
static const uint8_t RESPONSE = 0x42;
static const uint8_t RESPONSE_EXT = 0x56;  // same layout + 2 header bytes before value
static const uint8_t GET_REQ = 0x40;
static const uint8_t SET_REQ = 0x46;
static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;
static constexpr size_t MIN_MESSAGE_LEN = 5;  // cmd | fg | fn | dp_hi | dp_lo
static constexpr size_t MAX_PENDING_MESSAGES = 32;
static constexpr uint32_t MAX_PENDING_AGE_MS = 5000;
static constexpr uint32_t CLEANUP_INTERVAL_MS = 5000;
TopTronic::TopTronic(canbus::Canbus *canbus) : canbus_(canbus) {
  this->canbus_->add_callback([this](uint32_t can_id, bool, bool rtr, const std::vector<uint8_t> &data) {
    if (((can_id >> 11) & 0x7FF) == this->get_device_id())
      this->parse_frame(data, can_id, rtr);
  });
}
uint32_t build_can_id(uint16_t sender_id, uint16_t receiver_mask) {
  return (0x7F << 22) | (sender_id << 11) | receiver_mask;
}
std::vector<uint8_t> build_get_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint) {
  return {0x01, GET_REQ, function_group, function_number, (uint8_t) (datapoint >> 8), (uint8_t) datapoint};
}
std::vector<uint8_t> build_set_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint,
                                       const std::vector<uint8_t> &value) {
  std::vector<uint8_t> data = {
      0x01, SET_REQ, function_group, function_number, (uint8_t) (datapoint >> 8), (uint8_t) datapoint};
  data.insert(data.end(), value.begin(), value.end());
  return data;
}
static uint32_t reflect(uint32_t val, uint8_t width) {
  uint32_t out = 0;
  for (uint8_t i = 0; i < width; ++i) {
    out = (out << 1) | (val & 1);
    val >>= 1;
  }
  return out;
}
// CRC-16/poly 0x1021, init 0xB006, refin/refout, no xorout (from bus captures).
static const uint16_t *crc16_table() {
  static uint16_t table[256];
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < 256; ++i) {
      uint16_t v = static_cast<uint16_t>(i << 8);
      for (int b = 0; b < 8; ++b)
        v = (v & 0x8000) ? static_cast<uint16_t>((v << 1) ^ 0x1021) : static_cast<uint16_t>(v << 1);
      table[i] = v;
    }
    initialized = true;
  }
  return table;
}
static uint16_t compute_crc16(const uint8_t *data, size_t len) {
  const uint16_t *const table = crc16_table();
  uint16_t crc = 0xB006;
  for (size_t i = 0; i < len; ++i) {
    uint8_t byte = static_cast<uint8_t>(reflect(data[i], 8));
    crc = static_cast<uint16_t>((crc << 8) ^ table[((crc >> 8) ^ byte) & 0xFF]);
  }
  return static_cast<uint16_t>(reflect(crc, 16));
}
void TopTronic::setup() { ESP_LOGI(TAG, "Hub 0x%04X", (unsigned) this->get_device_id()); }
void TopTronic::loop() {
  const uint32_t now = millis();
  if (now - this->last_cleanup_ms_ < CLEANUP_INTERVAL_MS)
    return;
  this->last_cleanup_ms_ = now;
  for (auto it = this->pending_messages_.begin(); it != this->pending_messages_.end();) {
    if (now - it->second.last_update_ms > MAX_PENDING_AGE_MS) {
      it = this->pending_messages_.erase(it);
    } else {
      ++it;
    }
  }
}
void TopTronic::dump_config() { ESP_LOGCONFIG(TAG, "TopTronic device 0x%04X", (unsigned) this->get_device_id()); }
void TopTronic::parse_frame(const std::vector<uint8_t> &data, uint32_t can_id, bool remote_transmission_request) {
  (void) remote_transmission_request;
  uint8_t msg_id = can_id >> 24;
  uint32_t device_id = (can_id >> 11) & 0x7FF;
  const uint32_t now = millis();
  if (device_id != this->get_device_id())
    return;
  if (msg_id == START_OF_MESSAGE_ID) {
    if (data.size() < 2)
      return;
    uint8_t num_remaining = data[0] >> 3;
    if (num_remaining > MAX_PENDING_MESSAGES || num_remaining == 1)
      return;
    if (num_remaining == 0) {
      this->interpret_message_(data.data() + 1, data.size() - 1, can_id);
    } else {
      uint32_t header_key = (device_id << 8) | data[1];
      if (this->pending_messages_.size() >= MAX_PENDING_MESSAGES && !this->pending_messages_.contains(header_key))
        this->pending_messages_.erase(this->pending_messages_.begin());
      PendingMessage pending;
      pending.data.reserve(static_cast<size_t>(num_remaining) * MAX_CONT_FRAME_PAYLOAD);
      pending.data.assign(data.begin() + 2, data.end());
      pending.remaining_frames = num_remaining - 1;
      pending.last_update_ms = now;
      this->pending_messages_[header_key] = std::move(pending);
    }
  } else {
    if (data.size() < 2)
      return;
    uint32_t header_key = (device_id << 8) | data[0];
    auto it = this->pending_messages_.find(header_key);
    if (it == this->pending_messages_.end() || it->second.remaining_frames == 0) {
      if (it != this->pending_messages_.end())
        this->pending_messages_.erase(it);
      return;
    }
    PendingMessage &pending = it->second;
    pending.data.insert(pending.data.end(), data.begin() + 1, data.end());
    pending.last_update_ms = now;
    pending.remaining_frames--;
    if (pending.remaining_frames == 0) {
      const size_t msg_len = pending.data.size();
      if (msg_len < MIN_MESSAGE_LEN + 2) {
        this->pending_messages_.erase(it);
        return;
      }
      const uint8_t *msg = pending.data.data();
      uint16_t received_crc = (msg[msg_len - 2] << 8) | msg[msg_len - 1];
      if (received_crc != compute_crc16(msg, msg_len - 2)) {
        ESP_LOGW(TAG, "CRC check failed");
        this->pending_messages_.erase(it);
        return;
      }
      this->interpret_message_(msg, msg_len - 2, can_id);
      this->pending_messages_.erase(it);
    }
  }
}
// [0] cmd (0x40 GET, 0x46 SET, 0x42/0x56 RESPONSE), [1] fg, [2] fn,
// [3..4] datapoint, [5..] value (0x56 value starts at [7]).
void TopTronic::interpret_message_(const uint8_t *data, size_t len, uint32_t can_id) {
  if (len < MIN_MESSAGE_LEN || data[0] == GET_REQ || data[0] == SET_REQ)
    return;
  if (data[0] != RESPONSE && data[0] != RESPONSE_EXT)
    return;
  const size_t value_off = (data[0] == RESPONSE_EXT) ? 7 : 5;
  if (len < value_off)
    return;
  ESP_LOGV(TAG, "Response 0x%02X from 0x%03X: fg=%u fn=%u datapoint=%u, %zu value bytes", data[0],
           (unsigned) ((can_id >> 11) & 0x7FF), (unsigned) data[1], (unsigned) data[2],
           (unsigned) ((data[3] << 8) | data[4]), len - value_off);
}
}  // namespace esphome::toptronic

#include "toptronic.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::toptronic {

ESPHOME_LOG_TAG(TAG, "toptronic");

static constexpr uint8_t RESPONSE = 0x42;
static constexpr uint8_t RESPONSE_EXT = 0x56;  // same layout + 2 header bytes before value
static constexpr uint8_t GET_REQ = 0x40;
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;
static constexpr size_t MIN_MESSAGE_LEN = 5;  // cmd | fg | fn | dp_hi | dp_lo
static constexpr size_t CRC_LEN = 2;
// Keep in sync with DEVICE_TYPES["GW"] in __init__.py: the low 6 bits carry the hub address
static constexpr uint16_t GATEWAY_DEVICE_TYPE = 1152;  // GW
// Device types are 64 apart, so the low 6 bits of a device id are its address
static constexpr uint16_t DEVICE_ADDR_MASK = 0x3F;
static_assert((GATEWAY_DEVICE_TYPE & DEVICE_ADDR_MASK) == 0, "gateway type must leave the address bits clear");

static constexpr uint16_t sender_id(uint32_t can_id) { return (can_id >> 11) & 0x7FF; }

static constexpr uint32_t build_can_id(uint16_t sender_id, uint16_t receiver_mask) {
  return (0x7Fu << 22) | (static_cast<uint32_t>(sender_id) << 11) | receiver_mask;
}

static constexpr GetRequest build_get_request(uint8_t function_group, uint8_t function_number, uint16_t datapoint) {
  return {0x01,
          GET_REQ,
          function_group,
          function_number,
          static_cast<uint8_t>(datapoint >> 8),
          static_cast<uint8_t>(datapoint)};
}

TopTronic::TopTronic(canbus::Canbus *canbus, uint16_t device_id) : canbus_(canbus), device_id_(device_id) {
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

uint32_t TopTronicBase::get_id() {
  return this->function_group_ + (this->function_number_ << 8) + (static_cast<uint32_t>(this->datapoint_) << 16);
}
void TopTronicBase::cache_request_data() {
  this->request_data_ = build_get_request(this->function_group_, this->function_number_, this->datapoint_);
}
void TopTronicBase::add_on_update_callback(std::function<void()> &&callback) {
  this->update_callback_.add(std::move(callback));
}
void TopTronicBase::update() { this->update_callback_.call(); }

template<typename T> T bytes_to_number(const uint8_t *data, size_t len) {
  uint64_t u = 0;
  for (size_t i = 0; i < len; i++)
    u = (u << 8) | data[i];
  return static_cast<T>(u);
}

float TopTronicSensor::parse_value(const uint8_t *data, size_t len) {
  switch (this->type_) {
    case U8:
      return (float) bytes_to_number<uint8_t>(data, len);
    case U16:
      return (float) bytes_to_number<uint16_t>(data, len);
    case U32:
      return (float) bytes_to_number<uint32_t>(data, len);
    case S8:
      return (float) bytes_to_number<int8_t>(data, len);
    case S16:
      return (float) bytes_to_number<int16_t>(data, len);
    case S32:
      return (float) bytes_to_number<int32_t>(data, len);
    case S64:
      return (float) bytes_to_number<int64_t>(data, len);
  }
  return 0.0f;
}
size_t TopTronicSensor::value_width() const {
  switch (this->type_) {
    case U8:
    case S8:
      return 1;
    case U16:
    case S16:
      return 2;
    case U32:
    case S32:
      return 4;
    case S64:
      return 8;
  }
  return 1;
}

void TopTronic::add_sensor(TopTronicBase *sensor) {
  const uint32_t id = sensor->get_id();
  if (auto existing = this->sensors_.find(id); existing != this->sensors_.end()) {
    // Config validation rejects two read entities on one datapoint, so only external components get here
    ESP_LOGW(TAG, "Duplicate datapoint: '%s' replaces '%s'", LOG_STR_ARG(sensor->get_log_name()),
             LOG_STR_ARG(existing->second->get_log_name()));
  }
  this->sensors_[id] = sensor;
}

void TopTronic::register_sensor_callbacks() {
  auto *canbus = this->canbus_;
  const uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | (this->device_id_ & DEVICE_ADDR_MASK), this->device_id_);
  for (const auto &s : this->sensors_) {
    auto *sensor = s.second;
    sensor->cache_request_data();
    sensor->add_on_update_callback([canbus, sensor, can_id]() -> void {
      const auto &data = sensor->get_request_data();
      canbus->send_data(can_id, true, false, data.data(), data.size());
    });
  }
}

void TopTronic::setup() {
  // Wire the GET callbacks here, after every platform's add_sensor() has run.
  this->register_sensor_callbacks();
  ESP_LOGI(TAG, "Hub 0x%04X (%zu sensors)", (unsigned) this->device_id_, this->sensors_.size());
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
  uint32_t id = data[1] + (data[2] << 8) + (static_cast<uint32_t>(encode_uint16(data[3], data[4])) << 16);
  auto sensor_it = this->sensors_.find(id);
  if (sensor_it == this->sensors_.end())
    return;
  TopTronicBase *sensor_base = sensor_it->second;
  const size_t value_len = len - value_off;
  size_t required_value_len = 1;
  if (sensor_base->type() == SENSOR)
    required_value_len = static_cast<TopTronicSensor *>(sensor_base)->value_width();
  if (value_len < required_value_len)
    return;
  auto *sensor = static_cast<TopTronicSensor *>(sensor_base);
  sensor->publish_state(sensor->parse_value(data + value_off, required_value_len));
}

}  // namespace esphome::toptronic

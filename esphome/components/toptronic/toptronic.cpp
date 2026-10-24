#include "toptronic.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace esphome::toptronic {

ESPHOME_LOG_TAG(TAG, "toptronic");

static constexpr uint8_t RESPONSE = 0x42;
static constexpr uint8_t RESPONSE_EXT = 0x56;  // same layout + 2 header bytes before value
static constexpr uint8_t GET_REQ = 0x40;
static constexpr uint8_t SET_REQ = 0x46;
static constexpr size_t MAX_FIRST_FRAME_PAYLOAD = 6;
static constexpr uint32_t CONTINUATION_ID_MASK = 0x003FFFFF;
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;
static constexpr size_t MIN_MESSAGE_LEN = 5;  // cmd | fg | fn | dp_hi | dp_lo
static constexpr size_t CRC_LEN = 2;
static constexpr uint16_t GATEWAY_DEVICE_TYPE = 1153;  // GW
// Device types are 64 apart, so the low 6 bits of a device id are its address
static constexpr uint16_t DEVICE_ADDR_MASK = 0x3F;

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

/// Writes the SET request for value into out and returns its length.
static constexpr size_t build_set_request(uint8_t function_group, uint8_t function_number, uint16_t datapoint,
                                          const uint8_t *value, size_t value_len, SetRequest &out) {
  const GetRequest header = build_get_request(function_group, function_number, datapoint);
  for (size_t i = 0; i < GET_REQUEST_LEN; i++)
    out[i] = header[i];
  out[1] = SET_REQ;
  value_len = std::min(value_len, MAX_SET_VALUE_LEN);
  for (size_t i = 0; i < value_len; i++)
    out[GET_REQUEST_LEN + i] = value[i];
  return GET_REQUEST_LEN + value_len;
}

TopTronic::TopTronic(canbus::Canbus *canbus, uint16_t device_id) : canbus_(canbus), device_id_(device_id) {
  canbus->add_callback([this](uint32_t can_id, bool extended_id, bool, const std::vector<uint8_t> &data) {
    if (!extended_id || sender_id(can_id) != this->device_id_)
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
void TopTronicBase::add_on_set_callback(std::function<void(const uint8_t *, size_t)> &&callback) {
  this->set_callback_.add(std::move(callback));
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

static size_t type_width(TypeName type) {
  switch (type) {
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

// Writes value big endian into out and returns its width. Rounds before narrowing (a scaled 21.5 must
// not become 21.4) and clamps to the wire type so an out-of-range write cannot wrap to an unrelated value.
static size_t float_to_bytes(float value, TypeName type, uint8_t *out) {
  int64_t v = static_cast<int64_t>(llroundf(value));
  switch (type) {
    case U8:
      v = std::clamp<int64_t>(v, 0, UINT8_MAX);
      break;
    case U16:
      v = std::clamp<int64_t>(v, 0, UINT16_MAX);
      break;
    case U32:
      v = std::clamp<int64_t>(v, 0, UINT32_MAX);
      break;
    case S8:
      v = std::clamp<int64_t>(v, INT8_MIN, INT8_MAX);
      break;
    case S16:
      v = std::clamp<int64_t>(v, INT16_MIN, INT16_MAX);
      break;
    case S32:
      v = std::clamp<int64_t>(v, INT32_MIN, INT32_MAX);
      break;
    case S64:
      break;
  }
  const size_t width = type_width(type);
  for (size_t i = 0; i < width; i++)
    out[i] = static_cast<uint8_t>(v >> (8 * (width - 1 - i)));
  return width;
}

void TopTronicBase::send_set_(float value, TypeName type) {
  uint8_t bytes[MAX_SET_VALUE_LEN];
  const size_t width = float_to_bytes(value, type, bytes);
  SetRequest data{};
  const size_t len =
      build_set_request(this->function_group_, this->function_number_, this->datapoint_, bytes, width, data);
  this->set_callback_.call(data.data(), len);
}

#ifdef USE_SENSOR
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
size_t TopTronicSensor::value_width() const { return type_width(this->type_); }
#endif
#ifdef USE_NUMBER
void TopTronicNumber::control(float value) {
  float v = this->multiplier_ * value;
  this->send_set_(v, this->type_);
  ESP_LOGD(TAG, "[SET] %s: %f", LOG_STR_ARG(this->get_log_name()), v);
}
#endif
#ifdef USE_TEXT_SENSOR
std::string TopTronicTextSensor::parse_value(const uint8_t *data, size_t len) {
  auto it = this->to_text_.find(bytes_to_number<uint8_t>(data, len));
  if (it == this->to_text_.end()) {
    ESP_LOGW(TAG, "Unknown text sensor value");
    return "";
  }
  return it->second;
}
#endif
#ifdef USE_SELECT
void TopTronicSelect::control(const std::string &text) {
  auto it = this->to_value_.find(text);
  if (it == this->to_value_.end()) {
    ESP_LOGW(TAG, "[SET] Unknown option '%s'", text.c_str());
    return;
  }
  this->send_set_(static_cast<float>(it->second), this->type_);
}
#endif

void TopTronic::add_sensor(TopTronicBase *sensor) { this->device_.get_sensors()[sensor->get_id()] = sensor; }
void TopTronic::add_input(TopTronicBase *input) { this->device_.get_inputs()[input->get_id()] = input; }

void TopTronic::register_sensor_callbacks() {
  auto *canbus = this->canbus_;
  const uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | (this->device_id_ & DEVICE_ADDR_MASK), this->device_id_);
  for (const auto &s : this->device_.get_sensors()) {
    auto *sensor = s.second;
    sensor->cache_request_data();
    sensor->add_on_update_callback([canbus, sensor, can_id]() -> void {
      const auto &data = sensor->get_request_data();
      canbus->send_data(can_id, true, false, data.data(), data.size());
    });
  }
}

// A request longer than one frame goes out as a start frame plus continuation frames with a CRC appended,
// the same layout parse_frame_() reassembles.
void TopTronic::send_frames(canbus::Canbus *canbus, uint32_t can_id, const uint8_t *data, size_t len) {
  if (len <= 8) {
    canbus->send_data(can_id, true, false, data, len);
    return;
  }
  static uint8_t msg_counter = 0;
  uint8_t msg_header = (++msg_counter == 0) ? ++msg_counter : msg_counter;
  // Drop the single-frame header byte, append the CRC
  uint8_t msg[MAX_SET_REQUEST_LEN - 1 + CRC_LEN];
  const size_t body_len = std::min(len - 1, MAX_SET_REQUEST_LEN - 1);
  std::memcpy(msg, data + 1, body_len);
  const uint16_t crc = crc16(msg, body_len, 0x600D, 0x8408);
  msg[body_len] = static_cast<uint8_t>(crc >> 8);
  msg[body_len + 1] = static_cast<uint8_t>(crc & 0xFF);
  const size_t msg_len = body_len + CRC_LEN;
  const size_t first_chunk = std::min(MAX_FIRST_FRAME_PAYLOAD, msg_len);
  const size_t after_first = msg_len - first_chunk;
  const auto num_cont = static_cast<uint8_t>((after_first + MAX_CONT_FRAME_PAYLOAD - 1) / MAX_CONT_FRAME_PAYLOAD);
  uint8_t frame[8];
  frame[0] = static_cast<uint8_t>(((num_cont + 1) << 3) | 0x01);
  frame[1] = msg_header;
  std::memcpy(frame + 2, msg, first_chunk);
  canbus->send_data(can_id, true, false, frame, first_chunk + 2);
  const uint32_t cont_id = can_id & CONTINUATION_ID_MASK;
  for (size_t offset = first_chunk; offset < msg_len;) {
    const size_t chunk = std::min(MAX_CONT_FRAME_PAYLOAD, msg_len - offset);
    frame[0] = msg_header;
    std::memcpy(frame + 1, msg + offset, chunk);
    offset += chunk;
    canbus->send_data(cont_id, true, false, frame, chunk + 1);
  }
}

void TopTronic::register_input_callbacks() {
  auto *canbus = this->canbus_;
  const uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | (this->device_id_ & DEVICE_ADDR_MASK), this->device_id_);
  for (const auto &i : this->device_.get_inputs()) {
    i.second->add_on_set_callback(
        [canbus, can_id](const uint8_t *data, size_t len) -> void { send_frames(canbus, can_id, data, len); });
  }
}

void TopTronic::update_all() {
  for (const auto &s : this->device_.get_sensors())
    s.second->update();
}

TopTronicBase *TopTronic::get_sensor_(uint32_t sensor_id) {
  auto &sensors = this->device_.get_sensors();
  auto it = sensors.find(sensor_id);
  return it == sensors.end() ? nullptr : it->second;
}

void TopTronic::link_inputs_() {
  for (const auto &i : this->device_.get_inputs()) {
    auto *input_base = i.second;
    auto *sensor_base = this->get_sensor_(input_base->get_id());
    if (sensor_base == nullptr) {
      // A write-only input (for example a select with no readback) is valid, so this
      // stays at debug level rather than warning on every boot.
      ESP_LOGD(TAG, "Input 0x%08X has no matching read entity, state sync skipped", (unsigned) input_base->get_id());
      continue;
    }
    if (sensor_base->type() == SENSOR) {
#if defined(USE_SENSOR) && defined(USE_NUMBER)
      auto *sensor = static_cast<TopTronicSensor *>(sensor_base);
      auto *input = static_cast<TopTronicNumber *>(input_base);
      sensor->add_on_raw_state_callback(
          [input](float state) -> void { input->publish_state(state / input->get_multiplier()); });
#endif
    } else if (sensor_base->type() == TEXTSENSOR) {
#if defined(USE_TEXT_SENSOR) && defined(USE_SELECT)
      auto *sensor = static_cast<TopTronicTextSensor *>(sensor_base);
      auto *input = static_cast<TopTronicSelect *>(input_base);
      sensor->add_on_raw_state_callback([input](const std::string &state) -> void { input->publish_state(state); });
#endif
    }
  }
}

void TopTronic::setup() {
  // Wire the callbacks here, after every platform's add_sensor()/add_input() has run.
  this->link_inputs_();
  this->register_sensor_callbacks();
  this->register_input_callbacks();
  ESP_LOGI(TAG, "Hub 0x%04X (%zu sensors, %zu inputs)", (unsigned) this->device_id_, this->device_.get_sensors().size(),
           this->device_.get_inputs().size());
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
  auto &sensors = this->device_.get_sensors();
  auto sensor_it = sensors.find(id);
  if (sensor_it == sensors.end())
    return;
  TopTronicBase *sensor_base = sensor_it->second;
  const size_t value_len = len - value_off;
  size_t required_value_len = 1;
#ifdef USE_SENSOR
  if (sensor_base->type() == SENSOR)
    required_value_len = static_cast<TopTronicSensor *>(sensor_base)->value_width();
#endif
  if (value_len < required_value_len)
    return;
#ifdef USE_SENSOR
  if (sensor_base->type() == SENSOR) {
    auto *sensor = static_cast<TopTronicSensor *>(sensor_base);
    sensor->publish_state(sensor->parse_value(data + value_off, required_value_len));
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (sensor_base->type() == TEXTSENSOR) {
    auto *sensor = static_cast<TopTronicTextSensor *>(sensor_base);
    sensor->publish_state(sensor->parse_value(data + value_off, value_len));
  }
#endif
}

}  // namespace esphome::toptronic

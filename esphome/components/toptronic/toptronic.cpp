#include "toptronic.h"
#include "esphome/core/log.h"
#include <array>
#include <string>
namespace esphome::toptronic {
ESPHOME_LOG_TAG(TAG, "toptronic");
static constexpr uint8_t RESPONSE = 0x42;
static constexpr uint8_t RESPONSE_EXT = 0x56;  // same layout + 2 header bytes before value
static constexpr uint8_t GET_REQ = 0x40;
static constexpr uint8_t SET_REQ = 0x46;
static constexpr size_t MAX_FIRST_FRAME_PAYLOAD = 6;
static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
static constexpr uint32_t CONTINUATION_ID_MASK = 0x003FFFFF;
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;
static constexpr size_t MIN_MESSAGE_LEN = 5;  // cmd | fg | fn | dp_hi | dp_lo
static constexpr size_t MAX_PENDING_MESSAGES = 32;
static constexpr uint32_t MAX_PENDING_AGE_MS = 5000;
static constexpr uint32_t CLEANUP_INTERVAL_MS = 5000;
static constexpr uint16_t GATEWAY_DEVICE_TYPE = 1153;  // GW
TopTronic::TopTronic(canbus::Canbus *canbus) : canbus_(canbus) {
  this->canbus_->add_callback([this](uint32_t can_id, bool, bool rtr, const std::vector<uint8_t> &data) {
    if (this->owns_device_((can_id >> 11) & 0x7FF))
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
uint32_t TopTronicBase::get_id() {
  return this->function_group_ + (this->function_number_ << 8) + (this->datapoint_ << 16);
}
const std::vector<uint8_t> &TopTronicBase::get_request_data() { return this->request_data_; }
void TopTronicBase::cache_request_data() {
  this->request_data_ = build_get_request(this->function_group_, this->function_number_, this->datapoint_);
}
void TopTronicBase::add_on_set_callback(std::function<void(const std::vector<uint8_t> &)> &&callback) {
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
static std::vector<uint8_t> float_to_bytes(float value, TypeName type) {
  switch (type) {
    case U8:
      return {(uint8_t) value};
    case U16: {
      uint16_t v = (uint16_t) value;
      return {(uint8_t) (v >> 8), (uint8_t) v};
    }
    case U32: {
      uint32_t v = (uint32_t) value;
      return {(uint8_t) (v >> 24), (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v};
    }
    case S8:
      return {(uint8_t) (int8_t) value};
    case S16: {
      int16_t v = (int16_t) value;
      return {(uint8_t) (v >> 8), (uint8_t) v};
    }
    case S32: {
      int32_t v = (int32_t) value;
      return {(uint8_t) (v >> 24), (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v};
    }
    case S64: {
      int64_t v = (int64_t) value;
      std::array<uint8_t, 8> b{};
      for (size_t i = 0; i < 8; i++)
        b[i] = (uint8_t) (v >> (8 * (7 - i)));
      return {b.begin(), b.end()};
    }
  }
  return {};
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
#endif
#ifdef USE_NUMBER
void TopTronicNumber::control(float value) {
  float v = this->multiplier_ * value;
  std::vector<uint8_t> data = build_set_request(this->function_group_, this->function_number_, this->datapoint_,
                                                float_to_bytes(v, this->type_));
  this->set_callback_.call(data);
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
  std::vector<uint8_t> data = build_set_request(this->function_group_, this->function_number_, this->datapoint_,
                                                float_to_bytes((float) it->second, this->type_));
  this->set_callback_.call(data);
}
#endif
TopTronicDevice *TopTronic::get_or_create_device_(uint32_t device_id) {
  auto &device_ptr = this->devices_[device_id];
  if (!device_ptr)
    device_ptr = std::make_unique<TopTronicDevice>();
  return device_ptr.get();
}
void TopTronic::add_sensor(TopTronicBase *sensor) {
  this->get_or_create_device_(this->get_device_id())->get_sensors()[sensor->get_id()] = sensor;
}
void TopTronic::add_input(TopTronicBase *input) {
  this->get_or_create_device_(this->get_device_id())->get_inputs()[input->get_id()] = input;
}
void TopTronic::register_sensor_callbacks() {
  for (const auto &d : this->devices_) {
    for (const auto &s : d.second->get_sensors()) {
      auto *sensor = s.second;
      sensor->cache_request_data();
      auto *canbus = this->canbus_;
      uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | this->device_addr_, this->get_device_id());
      sensor->add_on_update_callback([canbus, sensor, can_id]() -> void {
        const auto &data = sensor->get_request_data();
        canbus->send_data(can_id, true, data);
      });
    }
  }
}
static void send_can_frames(canbus::Canbus *canbus, uint32_t can_id, const std::vector<uint8_t> &data) {
  if (data.size() <= 8) {
    canbus->send_data(can_id, true, data);
    return;
  }
  static uint8_t msg_counter = 0;
  uint8_t msg_header = (++msg_counter == 0) ? ++msg_counter : msg_counter;
  std::vector<uint8_t> msg(data.begin() + 1, data.end());
  uint16_t crc = crc16(msg.data(), msg.size(), 0x600D, 0x8408);
  msg.push_back(static_cast<uint8_t>(crc >> 8));
  msg.push_back(static_cast<uint8_t>(crc & 0xFF));
  size_t first_chunk = std::min<size_t>(MAX_FIRST_FRAME_PAYLOAD, msg.size());
  size_t after_first = msg.size() - first_chunk;
  auto num_cont = static_cast<uint8_t>((after_first + MAX_CONT_FRAME_PAYLOAD - 1) / MAX_CONT_FRAME_PAYLOAD);
  std::vector<uint8_t> first_frame;
  first_frame.push_back(static_cast<uint8_t>(((num_cont + 1) << 3) | 0x01));
  first_frame.push_back(msg_header);
  first_frame.insert(first_frame.end(), msg.begin(), msg.begin() + first_chunk);
  canbus->send_data(can_id, true, first_frame);
  uint32_t cont_id = can_id & CONTINUATION_ID_MASK;
  std::vector<uint8_t> cont_frame;
  cont_frame.reserve(8);
  for (size_t offset = first_chunk; offset < msg.size();) {
    size_t chunk = std::min<size_t>(MAX_CONT_FRAME_PAYLOAD, msg.size() - offset);
    cont_frame.clear();
    cont_frame.push_back(msg_header);
    cont_frame.insert(cont_frame.end(), msg.begin() + offset, msg.begin() + offset + chunk);
    offset += chunk;
    canbus->send_data(cont_id, true, cont_frame);
  }
}
void TopTronic::register_input_callbacks() {
  for (const auto &d : this->devices_) {
    for (const auto &i : d.second->get_inputs()) {
      auto *input = i.second;
      auto *canbus = this->canbus_;
      uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | this->device_addr_, d.first);
      input->add_on_set_callback(
          [canbus, can_id](const std::vector<uint8_t> &data) -> void { send_can_frames(canbus, can_id, data); });
    }
  }
}
void TopTronic::update_all() {
  for (const auto &d : this->devices_) {
    for (const auto &s : d.second->get_sensors())
      s.second->update();
  }
}
TopTronicBase *TopTronic::get_sensor_(uint32_t device_id, uint32_t sensor_id) {
  auto device_it = this->devices_.find(device_id);
  if (device_it == this->devices_.end())
    return nullptr;
  auto sensor_it = device_it->second->get_sensors().find(sensor_id);
  return (sensor_it == device_it->second->get_sensors().end()) ? nullptr : sensor_it->second;
}
void TopTronic::link_inputs_() {
  for (const auto &d : this->devices_) {
    for (const auto &i : d.second->get_inputs()) {
      auto *input_base = i.second;
      auto *sensor_base = this->get_sensor_(this->get_device_id(), input_base->get_id());
      if (sensor_base == nullptr) {
        // A write-only input (for example a select with no readback) is valid, so this
        // stays at debug level rather than warning on every boot.
        ESP_LOGD(TAG, "Input 0x%08X has no matching read entity — state sync skipped", (unsigned) input_base->get_id());
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
}
void TopTronic::setup() {
  // Wire the callbacks here, after every platform's add_sensor()/add_input() has run.
  this->link_inputs_();
  this->register_sensor_callbacks();
  this->register_input_callbacks();

  size_t sensors = 0, inputs = 0;
  for (const auto &d : this->devices_) {
    sensors += d.second->get_sensors().size();
    inputs += d.second->get_inputs().size();
  }
  ESP_LOGI(TAG, "Hub 0x%04X (%zu sensors, %zu inputs)", (unsigned) this->get_device_id(), sensors, inputs);

  this->set_interval(CLEANUP_INTERVAL_MS, [this]() {
    const uint32_t now = millis();
    for (auto it = this->pending_messages_.begin(); it != this->pending_messages_.end();) {
      if (now - it->second.last_update_ms > MAX_PENDING_AGE_MS) {
        it = this->pending_messages_.erase(it);
      } else {
        ++it;
      }
    }
  });
}
void TopTronic::dump_config() { ESP_LOGCONFIG(TAG, "TopTronic device 0x%04X", (unsigned) this->get_device_id()); }
void TopTronic::parse_frame(const std::vector<uint8_t> &data, uint32_t can_id, bool remote_transmission_request) {
  (void) remote_transmission_request;
  uint8_t msg_id = can_id >> 24;
  uint32_t device_id = (can_id >> 11) & 0x7FF;
  const uint32_t now = millis();
  if (!this->owns_device_(device_id))
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
      if (received_crc != crc16(msg, msg_len - 2, 0x600D, 0x8408)) {
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
  auto device_it = this->devices_.find((can_id >> 11) & 0x7FF);
  if (device_it == this->devices_.end())
    return;
  uint32_t id = data[1] + (data[2] << 8) + ((data[4] + (data[3] << 8)) << 16);
  auto sensor_it = device_it->second->get_sensors().find(id);
  if (sensor_it == device_it->second->get_sensors().end())
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

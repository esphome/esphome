#include "toptronic.h"
#include "esphome/core/log.h"
namespace esphome::toptronic {
ESPHOME_LOG_TAG(TAG, "toptronic");
static constexpr uint8_t RESPONSE = 0x42;
static constexpr uint8_t RESPONSE_EXT = 0x56;  // same layout + 2 header bytes before value
static constexpr uint8_t GET_REQ = 0x40;
static constexpr uint8_t SET_REQ = 0x46;
static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
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
TopTronicDevice *TopTronic::get_or_create_device_(uint32_t device_id) {
  auto &device_ptr = this->devices_[device_id];
  if (!device_ptr)
    device_ptr = std::make_unique<TopTronicDevice>();
  return device_ptr.get();
}
void TopTronic::add_sensor(TopTronicBase *sensor) {
  this->get_or_create_device_(this->get_device_id())->get_sensors()[sensor->get_id()] = sensor;
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
void TopTronic::setup() {
  // Wire the GET callbacks here, after every platform's add_sensor() has run.
  this->register_sensor_callbacks();

  size_t sensors = 0;
  for (const auto &d : this->devices_)
    sensors += d.second->get_sensors().size();
  ESP_LOGI(TAG, "Hub 0x%04X (%zu sensors)", (unsigned) this->get_device_id(), sensors);

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
}
}  // namespace esphome::toptronic

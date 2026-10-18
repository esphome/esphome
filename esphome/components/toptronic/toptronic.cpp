#include "toptronic.h"
#include "esphome/core/log.h"

#include <array>
#include <string>

namespace esphome::toptronic {

ESPHOME_LOG_TAG(TAG, "toptronic");

static const uint8_t RESPONSE = 0x42;
// 0x56 = extended-format RESPONSE (larger value payload, e.g. cleaning /
// maint. counters). Same layout as 0x42 but 2 extra bytes (0x80 0x00) between
// the datapoint and the value.
static const uint8_t RESPONSE_EXT = 0x56;
static const uint8_t GET_REQ = 0x40;
static const uint8_t SET_REQ = 0x46;

// Multi-frame protocol constants (see §3.5): a standard CAN frame holds 8 bytes;
// the first frame spends 2 on headers (6 payload bytes), continuation frames 1
// (7 payload bytes). Continuation CAN IDs clear bits 28-22 (msg_id → 0).
static constexpr size_t MAX_FIRST_FRAME_PAYLOAD = 6;
static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
static constexpr uint32_t CONTINUATION_ID_MASK = 0x003FFFFF;
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;

// Minimum decodable TopTronic message: cmd | function_group | function_number | dp_hi | dp_lo.
static constexpr size_t MIN_MESSAGE_LEN = 5;

// Constructor: arm the receive path immediately. All hubs are constructed in
// generated main.cpp before App.setup() runs, so even if a hub's setup() is
// deferred/stalled by another slow component, it can still receive and parse
// frames. (Sensor GET/set wiring stays in setup(): at construction time
// device_addr_/device_type_ are not yet set and devices_ is still empty.)
TopTronic::TopTronic(canbus::Canbus *canbus) : canbus_(canbus) {
  // Receive path: route every CAN frame to this hub when the sender node is one
  // of its devices. This does NOT depend on the sensor configuration (frames are
  // matched at receipt time), so it is safe to install before setup().
  this->canbus_->add_callback([this](uint32_t can_id, bool, bool rtr, const std::vector<uint8_t> &data) {
    if (this->owns_device_((can_id >> 11) & 0x7FF))
      this->parse_frame(data, can_id, rtr);
  });
}

// Logging shortcuts used throughout this component: one shared tag, and the level
// is chosen in one place instead of at every call site.
#define TT_LOGD(...) ESP_LOGD(TAG, __VA_ARGS__)
#define TT_LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define TT_LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)

// Format a byte buffer as a zero-padded hex string for log output (e.g. "4001001a").
// Uses a plain std::string (no std::stringstream) — common short messages fit in the
// small-string-optimization buffer, so the hot path (every received frame) performs
// no heap allocation.
std::string hex_str(const uint8_t *data, size_t len) {
  static const char *const HEX = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(HEX[(data[i] >> 4) & 0x0F]);
    out.push_back(HEX[data[i] & 0x0F]);
  }
  return out;
}

// The ESP32 CAN gateway presents itself on the bus as a TopTronic gateway (GW)
// device. This constant is the device-type portion of the sender CAN ID for all
// outgoing requests.
static constexpr uint16_t GATEWAY_DEVICE_TYPE = 1153;  // GW

// Build a 29-bit extended CAN ID using the TopTronic addressing scheme:
//   bits 28-22 : fixed 0x7F priority/type marker
//   bits 21-11 : sender node ID
//   bits 10-0  : receiver node ID (or broadcast mask)
uint32_t build_can_id(uint16_t sender_id, uint16_t receiver_mask) {
  return (0x7F << 22) | (sender_id << 11) | receiver_mask;
}

std::vector<uint8_t> build_get_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint) {
  return {
      0x01,     // message length
      GET_REQ,  // GET_REQUEST = 0x40
      function_group, function_number, (uint8_t) (datapoint >> 8), (uint8_t) (datapoint),
  };
}

// Build a SET request payload. The result may exceed 8 bytes for large value types
// (U32/S32 = 10 bytes, S64 = 14 bytes). The caller is responsible for splitting
// the payload into multiple CAN frames via send_can_frames() if needed.
std::vector<uint8_t> build_set_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint,
                                       const std::vector<uint8_t> &value) {
  std::vector<uint8_t> data = {
      0x01,     // message length / frame flags (upper 5 bits = 0 → single frame signal)
      SET_REQ,  // SET_REQUEST = 0x46
      function_group, function_number, (uint8_t) (datapoint >> 8), (uint8_t) (datapoint),
  };
  data.insert(data.end(), value.begin(), value.end());
  return data;
}

// Pack the three protocol fields into a single uint32 used as a lookup key.
// Layout: [datapoint(16) | function_number(8) | function_group(8)]
uint32_t TopTronicBase::get_id() {
  return this->function_group_ + (this->function_number_ << 8) + (this->datapoint_ << 16);
}

// Returns a const reference to avoid copying the vector on every polling cycle.
const std::vector<uint8_t> &TopTronicBase::get_request_data() { return this->request_data_; }

// Called once at setup time so subsequent poll callbacks read from a cached buffer.
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

// Decode a big-endian byte sequence into an integer of type T.
// Accumulates into uint64_t to avoid signed-shift UB when T is int64_t
// (left-shifting into the sign bit is undefined for signed types in C++).
// static_cast<T> of an out-of-range uint64_t is implementation-defined but
// produces the expected two's-complement result on all ESPHome targets.
template<typename T> T bytes_to_number(const uint8_t *data, size_t len) {
  uint64_t u = 0;
  for (size_t i = 0; i < len; i++) {
    u = (u << 8) | data[i];
  }
  return static_cast<T>(u);
}

// Byte width of a numeric datapoint value for the given TypeName. A RESPONSE
// must carry at least this many value bytes before its payload is decoded.
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

// Convert raw CAN bytes to a float, interpreting them as the configured integer type.
float bytes_to_float(const uint8_t *data, size_t len, TypeName type) {
  switch (type) {
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

// Encode a numeric value as a big-endian byte sequence for a SET request payload.
template<typename T> std::vector<uint8_t> number_to_bytes(T value) {
  std::array<uint8_t, sizeof(T)> bytes{};
  for (size_t i = 0; i < bytes.size(); i++) {
    bytes[i] = (uint8_t) (value >> (8 * (bytes.size() - i - 1)));
  }
  return {bytes.begin(), bytes.end()};
}

std::vector<uint8_t> float_to_bytes(float value, TypeName type) {
  switch (type) {
    case U8:
      return number_to_bytes((uint8_t) value);
    case U16:
      return number_to_bytes((uint16_t) value);
    case U32:
      return number_to_bytes((uint32_t) value);
    case S8:
      return number_to_bytes((int8_t) value);
    case S16:
      return number_to_bytes((int16_t) value);
    case S32:
      return number_to_bytes((int32_t) value);
    case S64:
      return number_to_bytes((int64_t) value);
  }
  return {};
}

#ifdef USE_SENSOR
float TopTronicSensor::parse_value(const uint8_t *data, size_t len) { return bytes_to_float(data, len, this->type_); }
// Minimum value bytes a complete RESPONSE for this sensor must carry.
size_t TopTronicSensor::value_width() const { return type_width(this->type_); }
#endif

#ifdef USE_NUMBER
void TopTronicNumber::control(float value) {
  float v = this->multiplier_ * value;
  std::vector<uint8_t> bytes = float_to_bytes(v, this->type_);

  std::vector<uint8_t> data = build_set_request(this->function_group_, this->function_number_, this->datapoint_, bytes);
  this->set_callback_.call(data);

  TT_LOGD("[SET] %s: %f, Data: 0x%s", this->get_name().c_str(), v, hex_str(data.data(), data.size()).c_str());
}
#endif

#ifdef USE_TEXT_SENSOR
std::string TopTronicTextSensor::parse_value(const uint8_t *data, size_t len) {
  uint8_t int_value = bytes_to_number<uint8_t>(data, len);
  auto it = this->to_text_.find(int_value);
  if (it == this->to_text_.end()) {
    TT_LOGW("Unknown text sensor value: %u", int_value);
    return "";
  }
  return it->second;
}
#endif

#ifdef USE_SELECT
void TopTronicSelect::control(const std::string &text) {
  auto it = this->to_value_.find(text);
  if (it == this->to_value_.end()) {
    TT_LOGW("[SET] Unknown option '%s' — ignoring", text.c_str());
    return;
  }
  uint8_t value = it->second;

  std::vector<uint8_t> data = build_set_request(this->function_group_, this->function_number_, this->datapoint_,
                                                float_to_bytes(static_cast<float>(value), this->type_));
  this->set_callback_.call(data);

  TT_LOGD("[SET] %s: %s, Data: 0x%s", this->get_name().c_str(), text.c_str(),
          hex_str(data.data(), data.size()).c_str());
}
#endif

// Return the TopTronicDevice for this ID, creating it on first access.
// operator[] performs a single map lookup rather than count() + operator[] (two lookups).
TopTronicDevice *TopTronic::get_or_create_device_(uint32_t device_id) {
  auto &device_ptr = this->devices_[device_id];
  if (!device_ptr) {
    device_ptr = std::make_unique<TopTronicDevice>();
  }
  return device_ptr.get();
}

void TopTronic::add_sensor(TopTronicBase *sensor) {
  TopTronicDevice *device = this->get_or_create_device_(this->get_device_id());
  device->get_sensors()[sensor->get_id()] = sensor;
}

void TopTronic::add_input(TopTronicBase *input) {
  TopTronicDevice *device = this->get_or_create_device_(this->get_device_id());
  device->get_inputs()[input->get_id()] = input;
}

void TopTronic::register_sensor_callbacks() {
  for (const auto &d : this->devices_) {
    auto *device = d.second.get();
    for (const auto &s : device->get_sensors()) {
      auto *sensor = s.second;
      sensor->cache_request_data();  // build once, avoid per-poll heap alloc
      auto *canbus = this->canbus_;
      uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | this->device_addr_, this->get_device_id());

      // Capture the receiver device id (e.g. 0x208 for HV+8, 0x408 for BM+8) so the
      // [GET] log shows exactly which bus device is being polled.
      uint16_t receiver_dev = this->get_device_id();
      // Capture this hub so the poll callback can be silenced during OTA: pause()
      // only dropped incoming frames, but the 30 s scheduler polls kept sending
      // GETs. Gating on paused_ here makes pause a true idle of outgoing CAN too.
      TopTronic *hub = this;
      sensor->add_on_update_callback([canbus, sensor, can_id, receiver_dev, hub]() -> void {
        if (hub->paused_)
          return;
        const auto &data = sensor->get_request_data();
        canbus->send_data(can_id, true, data);
        TT_LOGD("[GET] dev 0x%04X Data: 0x%s", receiver_dev, hex_str(data.data(), data.size()).c_str());
      });
    }
  }
}

static uint32_t reflect(uint32_t val, uint8_t width) {
  uint32_t out = 0;
  for (uint8_t i = 0; i < width; ++i) {
    out = (out << 1) | (val & 1);
    val >>= 1;
  }
  return out;
}

// CRC-16 used by the TopTronic multi-frame protocol.
// Parameters identified by brute-force search against captured bus traffic:
//   poly=0x1021  init=0xB006  refin=true  refout=true  xorout=0x0000
// This matches the CRC-16/ARC family with a non-standard init value.
//
// Lookup-table form: the bit-wise algorithm reflects each input byte, runs an
// MSB-first (left-shifting) poly 0x1021 loop, then reflects the final CRC. The
// table is therefore MSB-first over PRE-REFLECTED bytes:
//   crc = (crc << 8) ^ table[((crc >> 8) ^ reflect(byte)) & 0xFF]
// Equivalent to the bit-wise loop (validated against captured samples).
//
// Thread-safety: the lazy one-time init below is intentionally not atomic.
// ESPHome routes all component calls ({parse_frame, loop, update}) through the
// single main-loop task, so no two tasks can race on `initialized` in practice.
// Keep this function on the main loop task; do not call it from other FreeRTOS
// tasks without first adding a mutex or moving the init to setup().
static const uint16_t *crc16_table() {
  static uint16_t table[256];
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < 256; ++i) {
      uint16_t v = static_cast<uint16_t>(i << 8);
      for (int b = 0; b < 8; ++b) {
        v = (v & 0x8000) ? static_cast<uint16_t>((v << 1) ^ 0x1021) : static_cast<uint16_t>(v << 1);
      }
      table[i] = v;
    }
    initialized = true;
  }
  return table;
}

static uint16_t compute_crc16(const uint8_t *data, size_t len) {
  const uint16_t *const table = crc16_table();
  uint16_t crc = 0xB006;  // init
  for (size_t i = 0; i < len; ++i) {
    uint8_t byte = static_cast<uint8_t>(reflect(data[i], 8));
    crc = static_cast<uint16_t>((crc << 8) ^ table[((crc >> 8) ^ byte) & 0xFF]);
  }
  return static_cast<uint16_t>(reflect(crc, 16));  // refout=true, xorout=0x0000
}

// Send a SET request over CAN, transparently splitting into multiple frames when needed.
//
// A standard CAN frame holds at most 8 bytes. The TopTronic multi-frame protocol works as:
//   First frame  (msg_id = 0x1F): [frame_count<<3|flags, msg_header, payload[0..5]]
//   Cont. frames (msg_id ≠ 0x1F): [msg_header, payload[6..12], ...]
//   Last 2 bytes of the reassembled payload are the CRC-16 checksum (poly=0x1021, init=0xB006).
//
// The continuation CAN ID clears bits 28-22 (sets the top-priority field to 0) so that
// parse_frame() on the receiver treats those frames as continuations, not new messages.
static void send_can_frames(canbus::Canbus *canbus, uint32_t can_id, const std::vector<uint8_t> &data) {
  if (data.size() <= 8) {
    // Single-frame: payload fits in one CAN frame — send as-is.
    canbus->send_data(can_id, true, data);
    return;
  }

  // Multi-frame: strip the first byte (0x01 single-frame flag) and reframe.
  // A static counter provides a unique msg_header key for each outgoing multi-frame message.
  static uint8_t msg_counter = 0;
  uint8_t msg_header = (++msg_counter == 0) ? ++msg_counter : msg_counter;  // skip 0

  // Build the full message payload, then append the 2-byte CRC (big-endian).
  std::vector<uint8_t> msg(data.begin() + 1, data.end());
  uint16_t crc = compute_crc16(msg.data(), msg.size());
  msg.push_back(static_cast<uint8_t>(crc >> 8));    // CRC high byte
  msg.push_back(static_cast<uint8_t>(crc & 0xFF));  // CRC low byte

  // First frame carries up to 6 message bytes (2 header bytes consume slots 0 and 1).
  size_t first_chunk = std::min<size_t>(MAX_FIRST_FRAME_PAYLOAD, msg.size());
  size_t after_first = msg.size() - first_chunk;
  auto num_cont = static_cast<uint8_t>((after_first + MAX_CONT_FRAME_PAYLOAD - 1) / MAX_CONT_FRAME_PAYLOAD);  // ceil

  // The first-frame header carries the TOTAL frame count (first frame +
  // continuations), matching the boiler's receive convention.
  std::vector<uint8_t> first_frame;
  first_frame.push_back(static_cast<uint8_t>(((num_cont + 1) << 3) | 0x01));
  first_frame.push_back(msg_header);  // reassembly key
  first_frame.insert(first_frame.end(), msg.begin(), msg.begin() + first_chunk);
  canbus->send_data(can_id, true, first_frame);

  // Continuation frames use a lower-priority CAN ID (bits 28-22 cleared → msg_id ≠ 0x1F).
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

  TT_LOGD("[SET] Sent %u CAN frames (msg_header=0x%02X, payload=%zu bytes)", 1 + num_cont, msg_header, data.size());
}

void TopTronic::register_input_callbacks() {
  for (const auto &d : this->devices_) {
    auto *device = d.second.get();
    uint32_t device_id = d.first;
    for (const auto &i : device->get_inputs()) {
      auto *input = i.second;
      auto *canbus = this->canbus_;
      uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | this->device_addr_, device_id);

      input->add_on_set_callback([this, canbus, can_id, device_id, input](const std::vector<uint8_t> &data) -> void {
        const uint32_t id = input->get_id();
        const uint32_t now = millis();

        // Write-safety rate limit: ignore repeats of the same datapoint that
        // arrive faster than write_min_interval_ms_ (default 2000 ms). This
        // protects the 50 kbps bus and the boiler controller from rapid SET
        // spamming (e.g. a misbehaving HA automation).
        if (this->write_min_interval_ms_ > 0) {
          const auto last = this->last_write_ms_.find(id);
          if (last != this->last_write_ms_.end() && (now - last->second) < this->write_min_interval_ms_) {
            TT_LOGW("[SET] %s rate-limited (min %u ms between writes to this datapoint)", input->get_name().c_str(),
                    (unsigned) this->write_min_interval_ms_);
            return;
          }
        }

        // Write-safety cold-cache guard: do not SET a datapoint that has never
        // answered a GET since boot (we would be writing blind). Datapoints with
        // no registered read sensor — e.g. the HV filter-maintenance button —
        // are exempt because there is nothing to have read.
        if (this->reject_writes_before_read_) {
          TopTronicBase *sensor = this->get_sensor_(device_id, id);
          if (sensor != nullptr && !this->read_ok_ids_.contains(id)) {
            TT_LOGW("[SET] %s rejected (datapoint never read since boot — cold cache)", input->get_name().c_str());
            return;
          }
        }

        this->last_write_ms_[id] = now;
        // send_can_frames handles single-frame (≤8 bytes) and multi-frame (>8 bytes) automatically.
        send_can_frames(canbus, can_id, data);
      });
    }
  }
}

// Request an immediate refresh from every registered sensor by firing its update
// callback (the same path the polling scheduler takes). Writable inputs (number/
// select) follow automatically through the linked sensors set up in link_inputs_().
void TopTronic::update_all() {
  for (const auto &d : this->devices_) {
    for (const auto &s : d.second->get_sensors())
      s.second->update();
  }
}

// Look up a sensor by its (device_id, sensor_id) pair.
// Uses find() on both maps so each is traversed at most once (no double-lookup).
TopTronicBase *TopTronic::get_sensor_(uint32_t device_id, uint32_t sensor_id) {
  auto device_it = this->devices_.find(device_id);
  if (device_it == this->devices_.end()) {
    return nullptr;  // device not registered — ignore
  }
  TopTronicDevice *device = device_it->second.get();

  auto sensor_it = device->get_sensors().find(sensor_id);
  if (sensor_it == device->get_sensors().end()) {
    return nullptr;  // sensor not registered for this device — ignore
  }
  return sensor_it->second;
}

void TopTronic::link_inputs_() {
  for (const auto &d : this->devices_) {
    auto *device = d.second.get();
    for (const auto &i : device->get_inputs()) {
      auto *input_base = i.second;
      auto *sensor_base = this->get_sensor_(this->get_device_id(), input_base->get_id());
      if (sensor_base == nullptr) {
        TT_LOGD("Input 0x%08X has no matching sensor — sync skipped", (unsigned) input_base->get_id());
        continue;
      }
      if (sensor_base->type() == SENSOR) {
#if defined(USE_SENSOR) && defined(USE_NUMBER)
        auto *sensor = static_cast<TopTronicSensor *>(sensor_base);
        auto *input = static_cast<TopTronicNumber *>(input_base);
        sensor->add_on_raw_state_callback([input](float state) -> void {
          float divider = input->get_multiplier();
          input->publish_state(state / divider);
        });
#endif  // USE_SENSOR && USE_NUMBER
      } else if (sensor_base->type() == TEXTSENSOR) {
#if defined(USE_TEXT_SENSOR) && defined(USE_SELECT)
        auto *sensor = static_cast<TopTronicTextSensor *>(sensor_base);
        auto *input = static_cast<TopTronicSelect *>(input_base);
        sensor->add_on_raw_state_callback([input](const std::string &state) -> void { input->publish_state(state); });
#endif  // USE_TEXT_SENSOR && USE_SELECT
      }
    }
  }
}

void TopTronic::pause() {
  this->paused_ = true;
  TT_LOGI("Hub 0x%04X paused (CAN processing and refresh bursts halted)", (unsigned) this->get_device_id());
}

void TopTronic::resume() {
  this->paused_ = false;
  TT_LOGI("Hub 0x%04X resumed", (unsigned) this->get_device_id());
}

void TopTronic::configure_hub() {
  // All functional wiring runs from the CONFIG phase (emitted by __init__.py
  // right after the add_sensor/add_input calls) instead of setup(). Reason:
  // ESPHome sizes components_ (StaticVector) from ESPHOME_COMPONENT_COUNT,
  // which is computed at CORE codegen priority - BEFORE this component
  // registers its preset-generated entity IDs in CORE.component_ids. The
  // under-count silently drops every registration past the StaticVector
  // capacity, so a hub can be missing from components_ entirely and its
  // setup() never invoked. Config-phase statements always run for every hub.

  this->link_inputs_();
  this->register_sensor_callbacks();
  this->register_input_callbacks();

  // A fresh boot must always start unpaused. Normally paused_ is already false,
  // but this is a safety net in case a previous session was interrupted while
  // paused (e.g. an OTA whose on_end/on_abort never ran): polls and refresh
  // bursts must not stay frozen across reboots.
  this->paused_ = false;
}

void TopTronic::setup() {
  // Functional wiring lives in configure_hub() (config phase) so it runs even
  // when ESPHome silently drops this hub from components_ (see configure_hub()).
  // setup() only keeps the observability log.
  size_t sensor_count = 0;
  size_t input_count = 0;
  for (const auto &d : this->devices_) {
    auto *device = d.second.get();
    sensor_count += device->get_sensors().size();
    input_count += device->get_inputs().size();
  }
  TT_LOGI("Hub 0x%04X registered (%zu sensors, %zu inputs)", (unsigned) this->get_device_id(), sensor_count,
          input_count);
}

void TopTronic::loop() {
  // Sweep stale multi-frame fragments. A message arrives as a start frame plus
  // continuations; if the boiler is reset (or a frame is lost) the remainder
  // never arrives and the partial buffer would otherwise sit in RAM forever.
  const uint32_t now = millis();
  if (now - this->last_cleanup_ms_ < this->cleanup_interval_ms_)
    return;
  this->last_cleanup_ms_ = now;
  for (auto it = this->pending_messages_.begin(); it != this->pending_messages_.end();) {
    if (now - it->second.last_update_ms > this->max_pending_age_ms_) {
      it = this->pending_messages_.erase(it);
    } else {
      ++it;
    }
  }
}

void TopTronic::dump_config() {
  size_t sensor_count = 0;
  size_t input_count = 0;
  for (const auto &d : this->devices_) {
    sensor_count += d.second->get_sensors().size();
    input_count += d.second->get_inputs().size();
  }
  ESP_LOGCONFIG(TAG, "TopTronic:");
  ESP_LOGCONFIG(TAG, "  Device type: 0x%04X, device address: 0x%02X", this->device_type_, this->device_addr_);
  ESP_LOGCONFIG(TAG, "  Devices: %u, sensors: %u, inputs: %u", (unsigned) this->devices_.size(),
                (unsigned) sensor_count, (unsigned) input_count);
  ESP_LOGCONFIG(TAG, "  Write safety: min SET interval %u ms, reject writes before read: %s",
                (unsigned) this->write_min_interval_ms_,
                this->reject_writes_before_read_ ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"));
}

static void log_response_frame(const uint8_t *data, size_t len, uint32_t can_id, const std::string &sensor_name) {
  TT_LOGD("[RES] Can-ID: 0x%08X, Sensor: %s, Data: 0x%s", (unsigned int) can_id, sensor_name.c_str(),
          hex_str(data, len).c_str());
}

// Handle a raw CAN frame from the bus.
//
// The TopTronic protocol uses two CAN ID ranges:
//   msg_id == 0x1F  →  start-of-message frame
//   other           →  continuation frame for a multi-frame message
//
// Short messages (msg_len == 0) fit in one frame and are dispatched immediately.
// Longer messages are split into multiple CAN frames and reassembled in pending_messages_
// using (source_device_id << 8 | msg_header) as the reassembly key. Once the expected
// number of continuation frames has arrived the message is dispatched.
void TopTronic::parse_frame(const std::vector<uint8_t> &data, uint32_t can_id, bool remote_transmission_request) {
  if (this->paused_) {
    return;  // OTA in progress — drop frames to keep the main loop and logging free
  }

  uint8_t msg_id = can_id >> 24;
  uint32_t device_id = (can_id >> 11) & 0x7FF;
  const uint32_t now = millis();

  // Fast path: this hub only cares about frames sent BY one of its own devices
  // (responses to its GETs). The sender node id is exactly the devices_ map key,
  // so a single lookup decides membership — everything else (other hubs' traffic,
  // boiler broadcasts from unregistered nodes) is skipped before any reassembly
  // or dispatch work (the same decision interpret_message_() would make later).
  // owns_device_() covers EVERY device registered on this hub, not one address.
  if (!this->owns_device_(device_id)) {
    return;
  }

  if (msg_id == START_OF_MESSAGE_ID) {
    // First frame of a message. data[0] upper 5 bits = number of remaining frames.
    if (data.size() < 2) {
      TT_LOGW("Dropping malformed start frame (%zu bytes)", data.size());
      return;
    }
    uint8_t num_remaining = data[0] >> 3;
    // The first-frame header holds the TOTAL frame count (first frame +
    // continuations). A legit U32/S32 response uses 2, S64 uses 3 — anything
    // claiming more than this->max_frames_per_message_ is a corrupted header. Reject it
    // immediately instead of reserving buffer space for up to 31 continuations.
    if (num_remaining > this->max_frames_per_message_) {
      // This frame is the documented benign "bogus 20-frame" boiler broadcast
      // (header 0xA1, see docs/candump_base.log §7) — not a TopTronic datapoint
      // response, and it never sends continuations. Rejection is correct, but
      // logged at DEBUG so it stays out of the WARN→main_logs path.
      TT_LOGD("Dropping start frame with implausible frame count %u (header 0x%02X)", num_remaining, data[0]);
      return;
    }
    if (num_remaining == 1) {
      // A start frame always begins a multi-frame message (total frame count >= 2).
      // total==1 is malformed: remaining_frames would start at 0, so no continuation
      // could ever complete it and the entry would sit as a zombie until the stale
      // sweep evicts it. Drop it like any other implausible header.
      TT_LOGD("Dropping start frame with total frame count 1 (header 0x%02X)", data[0]);
      return;
    }
    if (num_remaining == 0) {
      // Single-frame message: strip the length byte and dispatch directly.
      this->interpret_message_(data.data() + 1, data.size() - 1, can_id, remote_transmission_request);
    } else {
      // Multi-frame message: save the first fragment and wait for the rest.
      uint8_t msg_header = data[1];  // reassembly key shared across all frames of this message
      uint32_t header_key = (device_id << 8) | msg_header;
      TT_LOGD("     - Start of message with id: %d with length %d (Can-ID: 0x%08X, Data: 0x%s)", msg_header,
              num_remaining, (unsigned) can_id, hex_str(data.data(), data.size()).c_str());
      if (this->pending_messages_.size() >= this->max_pending_messages_ &&
          !this->pending_messages_.contains(header_key)) {
        // Buffer full: evict the SINGLE oldest entry (LRU) instead of clearing all
        // in-progress reassemblies. On a multi-hub bus every hub reassembles every
        // device's frames, so a full clear() would destroy the other hubs' pending
        // messages too — turning a full-buffer moment into lost responses until the
        // next 30 s poll. This map is capped and tiny (32 entries), so the linear
        // oldest-find is cheap and only runs on the full-buffer path.
        // Evict the single OLDEST entry. Compare wrap-safe unsigned AGE
        // (now - last_update_ms) instead of raw timestamps: raw comparisons are
        // wrong across the 32-bit millis() rollover and could erase a freshly
        // updated entry. Entries older than max_pending_age were already evicted
        // by the loop() sweep, so all ages here are far below the wrap window.
        uint32_t oldest_key = this->pending_messages_.begin()->first;
        uint32_t oldest_age = now - this->pending_messages_.begin()->second.last_update_ms;
        for (auto &pending_message : this->pending_messages_) {
          if (now - pending_message.second.last_update_ms > oldest_age) {
            oldest_key = pending_message.first;
            oldest_age = now - pending_message.second.last_update_ms;
          }
        }
        TT_LOGW("Pending message buffer full (%zu entries), evicting oldest 0x%08X", this->pending_messages_.size(),
                (unsigned int) oldest_key);
        this->pending_messages_.erase(oldest_key);
      }

      PendingMessage pending;
      // Pre-allocate FIRST so the initial fragment lands in the full-size buffer —
      // exactly ONE heap allocation per message (per §9.4). Each continuation frame
      // carries at most 7 payload bytes (8 - header byte); the reserve covers the
      // worst case, so the continuation insert()s never reallocate.
      pending.data.reserve(static_cast<size_t>(num_remaining) * MAX_CONT_FRAME_PAYLOAD);
      pending.data.assign(data.begin() + 2, data.end());
      // data[0]>>3 is the TOTAL frame count (first frame + continuations), so
      // only num_remaining - 1 continuation frames are expected. Verified against
      // captured bus traffic (command 0x42 responses to register 0x74).
      pending.remaining_frames = num_remaining - 1;
      pending.last_update_ms = now;
      this->pending_messages_[header_key] = std::move(pending);
    }
  } else {
    // Continuation frame: append payload to the in-progress message.
    if (data.size() < 2) {
      TT_LOGW("Dropping malformed continuation frame (%zu bytes)", data.size());
      return;
    }
    uint8_t msg_header = data[0];
    uint32_t header_key = (device_id << 8) | msg_header;
    auto it = this->pending_messages_.find(header_key);
    if (it == this->pending_messages_.end()) {
      // A frame whose msg_id (can_id bits 31-24) is 0x00 is a standalone
      // low-priority broadcast (device presence / ACK), NOT a continuation of a
      // 0x1f-start message — its "header" byte is just its own first payload
      // byte. Log it as its own class instead of a lost reassembly.
      if ((can_id >> 24) == 0) {
        TT_LOGD("[IGN] Low-priority frame from node 0x%03X (Can-ID: 0x%08X, Data: 0x%s)", (unsigned) device_id,
                (unsigned) can_id, hex_str(data.data(), data.size()).c_str());
      } else {
        // Continuation for an unknown/expired message (evicted, purged, or the start
        // frame never arrived). Logged at DEBUG so lost reassemblies are visible.
        TT_LOGD(
            "[DROP] Continuation for unknown/expired message (node 0x%03X, header 0x%02X) Can-ID: 0x%08X, Data: 0x%s",
            (unsigned) device_id, msg_header, (unsigned) can_id, hex_str(data.data(), data.size()).c_str());
      }
      return;
    }
    PendingMessage &pending = it->second;
    if (pending.remaining_frames == 0) {
      // Duplicate/extra continuation for an already-complete message — discard.
      TT_LOGD("[DROP] Extra continuation for complete message (node 0x%03X, header 0x%02X)", (unsigned) device_id,
              msg_header);
      this->pending_messages_.erase(it);
      return;
    }
    TT_LOGD("     - Part of message with id: %d with remaining length %d", msg_header, pending.remaining_frames - 1);
    pending.data.insert(pending.data.end(), data.begin() + 1, data.end());
    pending.last_update_ms = now;
    pending.remaining_frames--;

    if (pending.remaining_frames == 0) {
      const size_t msg_len = pending.data.size();
      if (msg_len < MIN_MESSAGE_LEN + 2) {
        TT_LOGW("Reassembled message too short for CRC (%zu bytes)", msg_len);
        this->pending_messages_.erase(it);
        return;
      }

      const uint8_t *msg = pending.data.data();
      uint16_t received_crc = (msg[msg_len - 2] << 8) | msg[msg_len - 1];
      uint16_t computed_crc = compute_crc16(msg, msg_len - 2);

      if (received_crc != computed_crc) {
        TT_LOGW("CRC check failed! Recv: 0x%04X, Comp: 0x%04X", received_crc, computed_crc);
        this->pending_messages_.erase(it);
        return;
      }

      // Dispatch first, then free the reassembly buffer (msg points into pending.data).
      this->interpret_message_(msg, msg_len - 2, can_id, remote_transmission_request);
      this->pending_messages_.erase(it);
    }
  }
}

// Dispatch a fully reassembled TopTronic message.
//
// Message byte layout (after the CAN framing bytes are stripped):
//   [0]   command byte  0x40 GET, 0x46 SET, 0x42/0x56 RESPONSE
//   [1]   function_group
//   [2]   function_number
//   [3]   datapoint high byte
//   [4]   datapoint low byte
//   [5..] value payload (0x42). 0x56 inserts 2 bytes (0x80 0x00) at [5..6],
//         so its value starts at [7].
void TopTronic::interpret_message_(const uint8_t *data, size_t len, uint32_t can_id, bool remote_transmission_request) {
  if (len < MIN_MESSAGE_LEN) {
    TT_LOGD("Message too short (%u bytes), ignoring", (unsigned) len);
    return;
  }

  // Ignore outgoing GET/SET requests that we echoed ourselves — nothing to update.
  if (data[0] == GET_REQ) {
    TT_LOGD("[GET] Can-ID: 0x%08X, Data: 0x%s", (unsigned int) can_id, hex_str(data, len).c_str());
    return;
  }

  if (data[0] == SET_REQ) {
    TT_LOGD("[SET] Can-ID: 0x%08X, Data: 0x%s", (unsigned int) can_id, hex_str(data, len).c_str());
    return;
  }

  bool is_response = (data[0] == RESPONSE || data[0] == RESPONSE_EXT);
  if (!is_response) {
    TT_LOGD("[UNK] Can-ID: 0x%08X, Data: 0x%s", (unsigned int) can_id, hex_str(data, len).c_str());
    return;
  }
  // 0x56 inserts 2 header bytes (0x80 0x00) between the datapoint and the value.
  const size_t value_off = (data[0] == RESPONSE_EXT) ? 7 : 5;
  if (len < value_off) {
    TT_LOGD("Response without value bytes (%u bytes), ignoring", (unsigned) len);
    return;
  }

  // The sender node ID sits in bits 21-11 of the CAN ID.
  uint32_t rx_device_id = (can_id >> 11) & 0x7FF;

  auto device_it = this->devices_.find(rx_device_id);
  if (device_it == this->devices_.end()) {
    // Message from a device we have no entities registered for. Logged at DEBUG
    // so unknown node ids (e.g. a misconfigured device_type tag) are visible
    // instead of silently dropping every frame from that device.
    TT_LOGD("[DROP] No device registered for node id 0x%03X, cmd=0x%02X fg=%u fn=%u dp_hi=0x%02X dp_lo=0x%02X",
            (unsigned) rx_device_id, data[0], data[1], data[2], data[3], data[4]);
    return;
  }
  TopTronicDevice *device = device_it->second.get();

  // Reconstruct the sensor lookup key from the protocol fields in the response.
  uint32_t datapoint = data[4] + (data[3] << 8);
  uint32_t id = data[1]           // function_group
                + (data[2] << 8)  // function_number
                + (datapoint << 16);

  auto sensor_it = device->get_sensors().find(id);
  if (sensor_it == device->get_sensors().end()) {
    // Unregistered datapoint for a known device. Logged at DEBUG so preset key
    // mismatches (fg/fn/dp vs. what the device actually emits) are visible
    // instead of silently dropping the response.
    TT_LOGD("[DROP] No sensor for key 0x%08X on node 0x%03X (fg=%u fn=%u dp=%u)", (unsigned) id,
            (unsigned) rx_device_id, data[1], data[2], (unsigned) datapoint);
    return;
  }
  TopTronicBase *sensor_base = sensor_it->second;

  // Reject truncated responses before they can unlock write safety or publish
  // a partial/zero value: the payload must carry at least as many value bytes
  // as the sensor's configured type decodes (1 for text sensors, 1/2/4/8 for
  // numeric types).
  const size_t value_len = len - value_off;
  size_t required_value_len = 1;  // text sensors decode a single uint8 code
#ifdef USE_SENSOR
  if (sensor_base->type() == SENSOR) {
    required_value_len = static_cast<TopTronicSensor *>(sensor_base)->value_width();
  }
#endif
  if (value_len < required_value_len) {
    TT_LOGW("Response for %s truncated (%u of %u value bytes), ignoring", sensor_base->get_name().c_str(),
            (unsigned) value_len, (unsigned) required_value_len);
    return;
  }

  // A RESPONSE with a complete value for this datapoint arrived, so the
  // cold-cache write guard (reject_writes_before_read_) may let SET requests
  // through from now on.
  this->read_ok_ids_.insert(id);

  // Downcast to the concrete type and publish the decoded value.
  // data[value_off..] contains the raw value bytes.
#ifdef USE_SENSOR
  if (sensor_base->type() == SENSOR) {
    auto *sensor = static_cast<TopTronicSensor *>(sensor_base);
    float value = sensor->parse_value(data + value_off, value_len);
    sensor->publish_state(value);
    log_response_frame(data, len, can_id, sensor->get_name());
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (sensor_base->type() == TEXTSENSOR) {
    auto *sensor = static_cast<TopTronicTextSensor *>(sensor_base);
    std::string value = sensor->parse_value(data + value_off, value_len);
    sensor->publish_state(value);
    log_response_frame(data, len, can_id, sensor->get_name());
  }
#endif
}

}  // namespace esphome::toptronic

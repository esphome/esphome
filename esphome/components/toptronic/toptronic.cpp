#include "toptronic.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>

namespace esphome::toptronic {

ESPHOME_LOG_TAG(TAG, "toptronic");
// Separate tag so a candump capture can be filtered from the normal hub log.
ESPHOME_LOG_TAG(CANDUMP_TAG, "candump");
static constexpr uint8_t RESPONSE = 0x42;
// 0x56 = extended-format RESPONSE (larger value payload, e.g. cleaning /
// maint. counters). Same layout as 0x42 but 2 extra bytes (0x80 0x00) between
// the datapoint and the value.
static constexpr uint8_t RESPONSE_EXT = 0x56;
static constexpr uint8_t GET_REQ = 0x40;
static constexpr uint8_t SET_REQ = 0x46;

// Multi-frame protocol constants (see §3.5): a standard CAN frame holds 8 bytes;
// the first frame spends 2 on headers (6 payload bytes), continuation frames 1
// (7 payload bytes). Continuation CAN IDs clear bits 28-22 (msg_id → 0).
static constexpr size_t MAX_FIRST_FRAME_PAYLOAD = 6;
static constexpr size_t MAX_CONT_FRAME_PAYLOAD = 7;
static constexpr uint32_t CONTINUATION_ID_MASK = 0x003FFFFF;
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;

// Minimum decodable TopTronic message: cmd | function_group | function_number | dp_hi | dp_lo.
static constexpr size_t MIN_MESSAGE_LEN = 5;

// Debug frame logging features. Each is an independent build-wide boolean flag,
// NOT per-hub and NOT mutually exclusive: with multiple toptronic hubs every hub
// receives every CAN frame, so logging must be deduplicated (see the single debug
// callback registered in setup()). Both flags reset to OFF on every boot, so they
// can never become permanent settings. The two debug switches are fully
// independent — each controls exactly one flag, and both can be active at the
// same time (candump floods the output, find_can_id still emits its WARN lines).
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_candump_enabled = false;
static bool s_find_can_id_enabled = false;
static bool s_debug_callback_registered = false;
static bool s_receive_callback_registered = false;
static uint32_t s_candump_start_ms = 0;
static uint32_t s_find_can_id_start_ms = 0;
static uint32_t s_last_candump_log_ms = 0;
static uint32_t s_candump_dropped = 0;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// Auto-off deadlines: debug modes are intended to be temporary. Both candump
// and find can_id self-disable after 120 s to protect network/log buffers. The
// deadline is checked on every received frame inside debug_log_frame().
static constexpr uint32_t CANDUMP_AUTO_OFF_MS = 120000;
static constexpr uint32_t FIND_CAN_ID_AUTO_OFF_MS = 120000;

// Longest reassembled message: the configured frame ceiling (31) times the
// continuation payload, plus the two CRC bytes. Sized for the hex log buffer.
static constexpr size_t MAX_MESSAGE_BYTES = 31 * MAX_CONT_FRAME_PAYLOAD + 2;
static constexpr size_t HEX_LOG_BUFFER_SIZE = 2 * MAX_MESSAGE_BYTES + 1;
// "XX XX ... XX\0": three characters per byte with the separator.
static constexpr size_t HEX_PRETTY_BUFFER_SIZE = 3 * MAX_MESSAGE_BYTES;

// Behavioral constants — named per §5.4 (no magic numbers).
static constexpr uint32_t CANDUMP_MIN_LOG_GAP_MS = 33;    // min ms between candump log lines
static constexpr uint32_t BURST_STALL_TIMEOUT_MS = 5000;  // refresh burst watchdog idle threshold
// How often the refresh burst is driven. The main loop ticks every ~16 ms, so a
// shorter timer cannot send GETs any faster than one per tick.
static constexpr uint32_t REFRESH_DRIVE_INTERVAL_MS = 20;
// The main loop's nominal period; the effective GET spacing never goes below it.
static constexpr uint32_t LOOP_INTERVAL_MS = 16;

// Fan-out for each debug flag: the matching TopTronicDebugSwitch registers here
// so its published switch state always mirrors the real (build-wide) flag.
CallbackManager<void(bool)> TopTronic::candump_update_callbacks;
CallbackManager<void(bool)> TopTronic::find_can_id_update_callbacks;

// ---------------------------------------------------------------------------
// Build-wide multi-hub refresh coordination.
//
// Every TopTronic hub registers itself in s_all_instances from its constructor,
// so a single refresh_all() call (button or boot) can fan out to ALL hubs. Each
// hub's update_all() batch is staggered by REFRESH_STAGGER_MS so the 50 kbps bus
// is not hit with a simultaneous burst of GET frames. The stagger is scheduled
// on the ESPHome main-loop task via set_timeout().
// ---------------------------------------------------------------------------
static std::vector<TopTronic *> s_all_instances;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr uint32_t REFRESH_STAGGER_MS = 15000;
// Forward declaration: referenced by the deduplicated debug callback installed
// in the constructor below (defined later in this file, next to the debug flags).
static void debug_log_frame(const std::vector<uint8_t> &data, uint32_t can_id);
// Forward declaration: logs the frames THIS gateway transmits (GET/SET requests)
// while candump is active, so a capture also shows the requested frames.
static void debug_log_tx_frame(const std::vector<uint8_t> &data, uint32_t can_id);
// One-shot post-boot refresh guard: the first hub's timer to expire runs the
// refresh for every hub, the rest are no-ops.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_boot_refresh_fired = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// Constructor: register this hub in the build-wide registry and arm the RECEIVE
// path immediately. All hubs are constructed in generated main.cpp before
// App.setup() runs, so even if a hub's setup() is deferred/stalled by another
// slow component, it can still receive and parse frames and is already known to
// refresh_all(). (Sensor GET/set wiring stays in setup(): at construction time
// device_addr_/device_type_ are not yet set and devices_ is still empty.)
TopTronic::TopTronic(canbus::Canbus *canbus) : canbus_(canbus) {
  s_all_instances.push_back(this);

  // Receive path: one build-wide callback routes every CAN frame to the hub(s)
  // that registered the sender node. This does NOT depend on device_/sensor
  // configuration (frames are matched at receipt time), so it is safe to install
  // before setup(). A single dispatch point (instead of one callback per hub)
  // keeps the per-frame cost of foreign traffic to a single hash lookup.
  if (!s_receive_callback_registered) {
    s_receive_callback_registered = true;
    this->canbus_->add_callback([](uint32_t can_id, bool, bool rtr, const std::vector<uint8_t> &data) {
      uint32_t device_id = (can_id >> 11) & 0x7FF;
      for (TopTronic *hub : s_all_instances) {
        // owns_device_() checks the hub's full devices_ map (every device it has
        // registered), so the sender is never limited to a single address.
        if (hub->owns_device_(device_id))
          hub->parse_frame(data, can_id, rtr);
      }
    });
  }
  // Deduplicated optional debug logging (candump / find can_id). Installed here
  // (once, build-wide) so it is also armed from the very first moment.
  if (!s_debug_callback_registered) {
    s_debug_callback_registered = true;
    this->canbus_->add_callback(
        [](uint32_t can_id, bool, bool, const std::vector<uint8_t> &data) { debug_log_frame(data, can_id); });
  }
}

// Runtime-gated logging: when CANDUMP debug is active, normal INFO/DEBUG output
// is silenced so the candump lines (tag "candump") are the only bus-related log
// traffic. FIND CAN-ID mode does NOT suppress normal output (it only adds WARN
// lines from debug_log_frame). Warnings are never silenced.
#define TT_LOGD(...) \
  do { \
    if (!s_candump_enabled) { \
      ESP_LOGD(TAG, __VA_ARGS__); \
    } \
  } while (0)
#define TT_LOGI(...) \
  do { \
    if (!s_candump_enabled) { \
      ESP_LOGI(TAG, __VA_ARGS__); \
    } \
  } while (0)

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
// the payload into multiple CAN frames via send_can_frames_() if needed.
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
uint32_t TopTronicDatapoint::get_id() const {
  return this->function_group_ + (this->function_number_ << 8) + (this->datapoint_ << 16);
}

// Called once at setup time so subsequent poll callbacks read from a cached buffer.
void TopTronicDatapoint::cache_request_data() {
  this->request_data_ = build_get_request(this->function_group_, this->function_number_, this->datapoint_);
}

void TopTronicDatapoint::add_on_set_callback(std::function<void(const std::vector<uint8_t> &)> &&callback) {
  this->set_callback_.add(std::move(callback));
}

void TopTronicDatapoint::add_on_update_callback(std::function<void()> &&callback) {
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

// Encode an integer value as a big-endian byte sequence for a SET request
// payload. The value is clamped to the target type's range first, so an
// out-of-range input cannot wrap around to an unintended value on the bus.
std::vector<uint8_t> number_to_bytes(int64_t value, TypeName type) {
  switch (type) {
    case U8:
      value = std::clamp<int64_t>(value, 0, UINT8_MAX);
      break;
    case U16:
      value = std::clamp<int64_t>(value, 0, UINT16_MAX);
      break;
    case U32:
      value = std::clamp<int64_t>(value, 0, UINT32_MAX);
      break;
    case S8:
      value = std::clamp<int64_t>(value, INT8_MIN, INT8_MAX);
      break;
    case S16:
      value = std::clamp<int64_t>(value, INT16_MIN, INT16_MAX);
      break;
    case S32:
      value = std::clamp<int64_t>(value, INT32_MIN, INT32_MAX);
      break;
    case S64:
      break;
  }
  const size_t width = type_width(type);
  std::vector<uint8_t> bytes(width);
  for (size_t i = 0; i < width; i++) {
    bytes[i] = static_cast<uint8_t>(static_cast<uint64_t>(value) >> (8 * (width - i - 1)));
  }
  return bytes;
}

std::vector<uint8_t> float_to_bytes(float value, TypeName type) {
  return number_to_bytes(static_cast<int64_t>(llroundf(value)), type);
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

  char hex[HEX_LOG_BUFFER_SIZE];
  format_hex_to(hex, data);
  ESP_LOGD(TAG, "[SET] %s: %f, Data: 0x%s", LOG_STR_ARG(this->get_log_name()), v, hex);
}
#endif

#ifdef USE_TEXT_SENSOR
std::string TopTronicTextSensor::parse_value(const uint8_t *data, size_t len) {
  uint8_t int_value = bytes_to_number<uint8_t>(data, len);
  auto it = this->to_text_.find(int_value);
  if (it == this->to_text_.end()) {
    ESP_LOGW(TAG, "Unknown text sensor value: %u", int_value);
    return "";
  }
  return it->second;
}
#endif

#ifdef USE_BUTTON
void TopTronicButton::press_action() {
  std::vector<uint8_t> bytes = number_to_bytes(this->value_, this->type_);
  std::vector<uint8_t> data = build_set_request(this->function_group_, this->function_number_, this->datapoint_, bytes);
  this->set_callback_.call(data);

  char hex[HEX_LOG_BUFFER_SIZE];
  format_hex_to(hex, data);
  TT_LOGD("[SET] %s: %lld, Data: 0x%s", LOG_STR_ARG(this->get_log_name()), (long long) this->value_, hex);
}

void TopTronicRefreshButton::press_action() { this->parent_->refresh_all(); }
#endif

#ifdef USE_SELECT
void TopTronicSelect::control(const std::string &text) {
  auto it = this->to_value_.find(text);
  if (it == this->to_value_.end()) {
    ESP_LOGW(TAG, "[SET] Unknown option '%s' — ignoring", text.c_str());
    return;
  }
  uint8_t value = it->second;

  std::vector<uint8_t> data = build_set_request(this->function_group_, this->function_number_, this->datapoint_,
                                                float_to_bytes(static_cast<float>(value), this->type_));
  this->set_callback_.call(data);

  char hex[HEX_LOG_BUFFER_SIZE];
  format_hex_to(hex, data);
  ESP_LOGD(TAG, "[SET] %s: %s, Data: 0x%s", LOG_STR_ARG(this->get_log_name()), text.c_str(), hex);
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

void TopTronic::add_sensor(TopTronicDatapoint *sensor) {
  TopTronicDevice *device = this->get_or_create_device_(this->get_device_id());
  device->get_sensors()[sensor->get_id()] = sensor;
}

void TopTronic::add_input(TopTronicDatapoint *input) {
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
        // While candump is active, export the request frame itself so the capture
        // shows the full request → response exchange (see debug_log_tx_frame()).
        debug_log_tx_frame(data, can_id);
        char hex[HEX_LOG_BUFFER_SIZE];
        format_hex_to(hex, data);
        TT_LOGD("[GET] dev 0x%04X Data: 0x%s", receiver_dev, hex);
      });
    }
  }
}
// Send a SET request over CAN, transparently splitting into multiple frames when needed.
//
// A standard CAN frame holds at most 8 bytes. The TopTronic multi-frame protocol works as:
//   First frame  (msg_id = 0x1F): [frame_count<<3|flags, msg_header, payload[0..5]]
//   Cont. frames (msg_id ≠ 0x1F): [msg_header, payload[6..12], ...]
//   Last 2 bytes of the reassembled payload are the CRC-16 checksum (poly=0x1021,
//   init=0xB006, refin/refout — via crc16(…, 0x600D, 0x8408)).
//
// The continuation CAN ID clears bits 28-22 (sets the top-priority field to 0) so that
// parse_frame() on the receiver treats those frames as continuations, not new messages.
canbus::Error TopTronic::send_can_frames_(uint32_t can_id, const std::vector<uint8_t> &data) {
  if (data.size() <= 8) {
    // Single-frame: payload fits in one CAN frame — send as-is.
    const canbus::Error result = this->canbus_->send_data(can_id, true, data);
    // Export the outgoing SET frame while candump is active.
    debug_log_tx_frame(data, can_id);
    return result;
  }

  // Multi-frame: strip the first byte (0x01 single-frame flag) and reframe.
  // The per-hub counter provides a unique msg_header key for each message.
  this->msg_counter_++;
  if (this->msg_counter_ == 0)
    this->msg_counter_++;  // skip 0
  const uint8_t msg_header = this->msg_counter_;

  // Build the full message payload, then append the 2-byte CRC (big-endian).
  std::vector<uint8_t> msg(data.begin() + 1, data.end());
  uint16_t crc = crc16(msg.data(), msg.size(), 0x600D, 0x8408);
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
  canbus::Error result = this->canbus_->send_data(can_id, true, first_frame);
  debug_log_tx_frame(first_frame, can_id);

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
    const canbus::Error err = this->canbus_->send_data(cont_id, true, cont_frame);
    debug_log_tx_frame(cont_frame, cont_id);
    if (result == canbus::ERROR_OK)
      result = err;
  }

  if (result != canbus::ERROR_OK) {
    ESP_LOGW(TAG, "[SET] Multi-frame send failed (canbus error %u, msg_header=0x%02X)", (unsigned) result, msg_header);
  } else {
    ESP_LOGD(TAG, "[SET] Sent %u CAN frames (msg_header=0x%02X, payload=%zu bytes)", 1 + num_cont, msg_header,
             data.size());
  }
  return result;
}

void TopTronic::register_input_callbacks() {
  for (const auto &d : this->devices_) {
    auto *device = d.second.get();
    uint32_t device_id = d.first;
    for (const auto &i : device->get_inputs()) {
      auto *input = i.second;
      uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | this->device_addr_, device_id);

      input->add_on_set_callback([this, can_id, device_id, input](const std::vector<uint8_t> &data) -> void {
        const uint32_t id = input->get_id();
        const uint32_t now = millis();

        // Write-safety rate limit: ignore repeats of the same datapoint that
        // arrive faster than write_min_interval_ms_ (default 2000 ms). This
        // protects the 50 kbps bus and the boiler controller from rapid SET
        // spamming (e.g. a misbehaving HA automation).
        if (this->write_min_interval_ms_ > 0) {
          const auto last = this->last_write_ms_.find(id);
          if (last != this->last_write_ms_.end() && (now - last->second) < this->write_min_interval_ms_) {
            ESP_LOGW(TAG, "[SET] %s rate-limited (min %u ms between writes to this datapoint)",
                     LOG_STR_ARG(input->get_log_name()), (unsigned) this->write_min_interval_ms_);
            return;
          }
        }

        // Write-safety cold-cache guard: do not SET a datapoint that has never
        // answered a GET since boot (we would be writing blind). Datapoints with
        // no registered read sensor — e.g. the HV filter-maintenance button —
        // are exempt because there is nothing to have read.
        if (this->reject_writes_before_read_) {
          TopTronicDatapoint *sensor = this->get_sensor_(device_id, id);
          if (sensor != nullptr && !this->read_ok_ids_.contains(id)) {
            ESP_LOGW(TAG, "[SET] %s rejected (datapoint never read since boot — cold cache)",
                     LOG_STR_ARG(input->get_log_name()));
            return;
          }
        }

        // Record the write only when the bus accepted every frame, so a failed
        // send does not block a retry for write_min_interval_ms_.
        const canbus::Error err = this->send_can_frames_(can_id, data);
        if (err != canbus::ERROR_OK) {
          ESP_LOGW(TAG, "[SET] %s failed (canbus error %u)", LOG_STR_ARG(input->get_log_name()), (unsigned) err);
          return;
        }
        this->last_write_ms_[id] = now;
      });
    }
  }
}

// Request an immediate refresh from every registered sensor by firing its update
// callback (the same path the polling scheduler takes). Writable inputs (number/
// select) follow automatically through the linked sensors set up in link_inputs_().
//
// The refresh is THROTTLED: sensors are queued into pending_refresh_ and released
// with an effective per-GET spacing of refresh_gap_ms_ / max_refresh_per_loop_
// preset does not saturate the 50 kbps bus with a burst of GET frames and does
// not compress the boiler's responses into a main-loop-stalling avalanche.
void TopTronic::update_all() {
  // A burst is already draining. Instead of dropping the request, defer it:
  // remember that a fresh refresh is wanted and start a new burst once the
  // current one empties — so a "Refresh all" press during a burst is honored,
  // never silently lost.
  if (!this->pending_refresh_.empty()) {
    this->refresh_pending_ = true;
    TT_LOGI("Refresh already in progress (%zu sensors pending) [paused=%d], will re-run after it finishes",
            this->pending_refresh_.size(), (int) this->paused_);
    return;  // drain_refresh_burst() kicks off the deferred run when the burst empties
  }
  size_t sensor_count = 0;
  for (const auto &d : this->devices_) {
    auto *device = d.second.get();
    sensor_count += device->get_sensors().size();
    for (const auto &s : device->get_sensors()) {
      RefreshEntry entry;
      entry.sensor = s.second;
      entry.last_send_ms = 0;  // attempts==0 → first send goes out immediately
      entry.attempts = 0;
      this->pending_refresh_.push_back(entry);
    }
  }
  // This queueing begins a throttled burst. Arm the observability counters
  // (completion log + stall watchdog) for it.
  this->burst_in_progress_ = true;
  this->burst_queued_ = sensor_count;
  this->burst_answered_ = 0;
  this->burst_dropped_ = 0;
  this->last_burst_progress_ms_ = millis();

  TT_LOGI("Refresh requested for device 0x%04X (%zu sensors) [paused=%d]", (unsigned) this->get_device_id(),
          sensor_count, (int) this->paused_);
  // Kick the scheduler-driven drain so the burst starts promptly even if the
  // hub's component loop() never runs.
  this->set_timeout("refresh_drain", 1, [this]() { this->drain_refresh_burst(); });
}

// Refresh EVERY registered hub, staggering each hub's batch by REFRESH_STAGGER_MS
// so the 50 kbps bus is not spammed. Called from the "Refresh all" button and by
// the (single) one-shot boot refresh. Rapid repeated presses simply push later
// batches further out via absolute cumulative delays — the bus still only sees
// one hub's batch at a time.
void TopTronic::refresh_all() {
  size_t total_sensors = 0;
  size_t hubs = 0;
  for (TopTronic *hub : s_all_instances) {
    size_t count = 0;
    for (const auto &d : hub->devices_) {
      count += d.second->get_sensors().size();
    }
    total_sensors += count;
    ++hubs;
  }
  TT_LOGI("Refresh-all scheduled for %zu hub(s), %zu sensors (stagger %u s)", hubs, total_sensors,
          (unsigned) (REFRESH_STAGGER_MS / 1000));

  uint32_t offset = 0;
  for (TopTronic *hub : s_all_instances) {
    if (offset > 0) {
      // Stagger: schedule this hub's batch after the previous ones. Keyed by the
      // hub's stable object address so repeated refreshes reschedule only this
      // hub's pending batch (device addresses are identical across hubs).
      hub->set_timeout(reinterpret_cast<uintptr_t>(hub), offset, [hub]() { hub->update_all(); });
    } else {
      hub->update_all();  // first hub runs immediately
    }
    offset += REFRESH_STAGGER_MS;
  }
}

// Look up a sensor by its (device_id, sensor_id) pair.
// Uses find() on both maps so each is traversed at most once (no double-lookup).
TopTronicDatapoint *TopTronic::get_sensor_(uint32_t device_id, uint32_t sensor_id) {
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
      if (input_base->type() == BUTTON) {
        continue;  // buttons are fire-and-forget — no linked sensor to sync
      }
      auto *sensor_base = this->get_sensor_(this->get_device_id(), input_base->get_id());
      if (sensor_base == nullptr) {
        ESP_LOGD(TAG, "Input 0x%08X has no matching sensor — sync skipped", (unsigned) input_base->get_id());
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
  ESP_LOGI(TAG, "Hub 0x%04X paused (CAN processing and refresh bursts halted)", (unsigned) this->get_device_id());
}

void TopTronic::resume() {
  this->paused_ = false;
  ESP_LOGI(TAG, "Hub 0x%04X resumed", (unsigned) this->get_device_id());
}

void TopTronic::setup() {
  // Wire the callbacks here, after every platform's add_sensor()/add_input() has run.
  this->link_inputs_();
  this->register_sensor_callbacks();
  this->register_input_callbacks();

  // A fresh boot must always start unpaused. Normally paused_ is already false,
  // but this is a safety net in case a previous session was interrupted while
  // paused (e.g. an OTA whose on_end/on_abort never ran): polls and refresh
  // bursts must not stay frozen across reboots.
  this->paused_ = false;

  // One-shot post-boot refresh. Every hub is already in s_all_instances (the
  // constructor registers it before App.setup() runs), so the first timer to
  // fire can fan out to all hubs safely.
  if (this->boot_refresh_delay_ms_ != 0) {
    this->set_timeout("boot_refresh", this->boot_refresh_delay_ms_, []() {
      if (s_boot_refresh_fired)
        return;
      s_boot_refresh_fired = true;
      if (!s_all_instances.empty())
        s_all_instances[0]->refresh_all();
    });
  }

  // Pace the refresh burst. The loop ticks every ~16 ms, so a faster timer
  // cannot send more often than one GET per tick.
  this->set_interval("refresh_drain", REFRESH_DRIVE_INTERVAL_MS, [this]() { this->drain_refresh_burst(); });

#ifdef USE_OTA_STATE_LISTENER
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif

  size_t sensor_count = 0;
  size_t input_count = 0;
  for (const auto &d : this->devices_) {
    auto *device = d.second.get();
    sensor_count += device->get_sensors().size();
    input_count += device->get_inputs().size();
  }
  TT_LOGI("Hub 0x%04X registered, total hubs: %zu (%zu sensors, %zu inputs)", (unsigned) this->get_device_id(),
          s_all_instances.size(), sensor_count, input_count);

  // Periodically evict stale multi-frame reassembly buffers so a lost fragment (or a
  // device going offline mid-message) cannot pin an entry forever. The map is tiny
  // (capped at max_pending_messages_), so the sweep runs on this coarse interval.
  this->set_interval("cleanup", this->cleanup_interval_ms_, [this]() {
    const uint32_t now = millis();
    for (auto it = this->pending_messages_.begin(); it != this->pending_messages_.end();) {
      if (now - it->second.last_update_ms > this->max_pending_age_ms_) {
        TT_LOGD("Expiring stale pending message 0x%08X (%zu bytes, %u frames remaining)", (unsigned int) it->first,
                it->second.data.size(), it->second.remaining_frames);
        it = this->pending_messages_.erase(it);
      } else {
        ++it;
      }
    }
  });
}

#ifdef USE_OTA_STATE_LISTENER
void TopTronic::on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *component) {
  if (state == ota::OTA_STARTED) {
    this->pause();
  } else if (state == ota::OTA_COMPLETED || state == ota::OTA_ABORT || state == ota::OTA_ERROR) {
    this->resume();
  }
}
#endif

void TopTronic::drain_refresh_burst() {
  // Time-gated refresh burst with per-sensor retry. max_refresh_per_loop_ is the
  // GET burst budget per refresh_gap_ms_ window, so the target per-GET spacing is
  // ceil(refresh_gap_ms_ / max_refresh_per_loop_). Raising max_refresh_per_loop_
  // sends more GETs per window; lowering it paces harder. The main loop ticks
  // every ~16 ms, so that interval is the floor: asking for a shorter spacing
  // only means "one GET per tick".
  //
  // Each entry is a sensor whose GET is still awaiting a response. An entry that
  // has not been answered within refresh_retry_interval_ms_ is re-sent, up to
  // max_refresh_retries_ extra times. interpret_message_() removes an entry as
  // soon as its response arrives, so only genuinely unanswered GETs are retried.
  // This makes the burst self-healing and independent of bus/hub ordering - a
  // single lost GET (e.g. BM 83-0-0 colliding with boiler broadcasts) is retried
  // instead of leaving the sensor unknown until the next 30 s poll.
  // Auto-disable debug modes if left on, to protect network/log buffers. The
  // frame path checks this on every received frame (the primary path while a
  // candump flood starves the loop task); this is the fallback for a quiet bus.
  const uint32_t loop_now = App.get_loop_component_start_time();
  if (s_candump_enabled && loop_now - s_candump_start_ms > CANDUMP_AUTO_OFF_MS) {
    this->set_candump_enabled(false);
  }
  if (s_find_can_id_enabled && loop_now - s_find_can_id_start_ms > FIND_CAN_ID_AUTO_OFF_MS) {
    this->set_find_can_id_enabled(false);
  }

  const uint32_t refresh_burst = (this->max_refresh_per_loop_ == 0) ? 1 : this->max_refresh_per_loop_;
  const uint32_t spacing = (this->refresh_gap_ms_ + refresh_burst - 1) / refresh_burst;
  const uint32_t effective_gap_ms = std::max(spacing, LOOP_INTERVAL_MS + 1);
  if (!this->paused_ && !this->pending_refresh_.empty()) {
    const uint32_t now = App.get_loop_component_start_time();
    if (now - this->last_refresh_send_ms_ >= effective_gap_ms) {
      RefreshEntry entry = this->pending_refresh_.front();
      const uint32_t since_last = now - entry.last_send_ms;
      if (entry.attempts == 0 || since_last >= this->refresh_retry_interval_ms_) {
        if (entry.attempts > this->max_refresh_retries_) {
          // Every allowed send went unanswered - give up on this sensor for this
          // burst. The normal 30 s poll remains the backstop, so a dead/absent
          // device is never hammered.
          this->pending_refresh_.pop_front();
          this->burst_dropped_++;  // counted in the completion log
          this->last_burst_progress_ms_ = now;
          TT_LOGD("[GET] Giving up device 0x%04X sensor 0x%08X after %u tries", (unsigned) this->get_device_id(),
                  (unsigned) entry.sensor->get_id(), (unsigned) entry.attempts);
        } else {
          if (entry.attempts > 0) {
            TT_LOGD("[GET] Retry %u/%u device 0x%04X sensor 0x%08X", (unsigned) entry.attempts,
                    (unsigned) this->max_refresh_retries_, (unsigned) this->get_device_id(),
                    (unsigned) entry.sensor->get_id());
          }
          entry.sensor->request_update();
          entry.last_send_ms = now;
          entry.attempts++;
          this->pending_refresh_.pop_front();
          // Re-queue so every other sensor is tried before this one is retried.
          this->pending_refresh_.push_back(entry);
          this->last_burst_progress_ms_ = now;  // burst made progress: a GET was sent
        }
        this->last_refresh_send_ms_ = now;
      }
    }
  }

  // Refresh-burst observability: log when a throttled burst finishes draining so
  // the log proves the queue is being processed. Accounting: queued = sensors
  // queued by update_all(), answered = responses erased in interpret_message_(),
  // dropped = retries exhausted or stall-aborted (watchdog below).
  if (this->burst_in_progress_ && this->pending_refresh_.empty()) {
    this->burst_in_progress_ = false;
    TT_LOGI("Refresh burst finished for device 0x%04X: %zu queued, %zu answered, %zu dropped",
            (unsigned) this->get_device_id(), this->burst_queued_, this->burst_answered_, this->burst_dropped_);
  }

  // Burst stall watchdog: a draining burst must keep making progress (a GET
  // sent, or a response erasing an entry) at roughly the retry cadence. If it
  // has been idle for 5 s + refresh_retry_interval_ms_, the burst is wedged
  // (loop task starved, bus dead, device gone silent). Abort it so a later
  // refresh can start fresh - the normal 30 s polls remain the backstop.
  // Skipped while paused_ (OTA) so an intentional freeze is not flagged.
  if (this->burst_in_progress_ && !this->paused_ && !this->pending_refresh_.empty()) {
    const uint32_t now = App.get_loop_component_start_time();
    const uint32_t stall_timeout = BURST_STALL_TIMEOUT_MS + this->refresh_retry_interval_ms_;
    if (now - this->last_burst_progress_ms_ >= stall_timeout) {
      const size_t abandoned = this->pending_refresh_.size();
      ESP_LOGW(TAG, "Refresh burst stalled (%zu sensors pending, no progress for %u ms) - aborting", abandoned,
               (unsigned) (now - this->last_burst_progress_ms_));
      this->pending_refresh_.clear();
      this->burst_dropped_ += abandoned;
      this->burst_in_progress_ = false;
      TT_LOGI("Refresh burst finished for device 0x%04X: %zu queued, %zu answered, %zu dropped (stall-aborted)",
              (unsigned) this->get_device_id(), this->burst_queued_, this->burst_answered_, this->burst_dropped_);
    }
  }

  // The burst has finished (either drained or empty). If a refresh was requested
  // while it was running, honor it now with a fresh burst - a "Refresh all" press
  // during a burst is deferred, never lost.
  if (this->refresh_pending_ && this->pending_refresh_.empty()) {
    this->refresh_pending_ = false;
    this->update_all();
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
  ESP_LOGCONFIG(TAG, "  Boot refresh delay: %u ms", (unsigned) this->boot_refresh_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Write safety: min SET interval %u ms, reject writes before read: %s",
                (unsigned) this->write_min_interval_ms_,
                this->reject_writes_before_read_ ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"));
}

static void log_response_frame(const uint8_t *data, size_t len, uint32_t can_id, const LogString *sensor_name) {
  char hex[HEX_LOG_BUFFER_SIZE];
  format_hex_to(hex, data, len);
  ESP_LOGD(TAG, "[RES] Can-ID: 0x%08X, Sensor: %s, Data: 0x%s", (unsigned int) can_id, LOG_STR_ARG(sensor_name), hex);
}

// ---------------------------------------------------------------------------
// Debug flags — single source of truth for enabling/disabling a debug feature.
//
// The state lives in build-wide static booleans; the fan-out to the matching
// TopTronicDebugSwitch goes through the per-feature static callback manager.
// Both the public TopTronic setters and the per-frame auto-off inside
// debug_log_frame() go through these helpers, so the switch state always ends
// up mirroring the real flag regardless of WHO disabled the feature (user
// toggle, auto-off timer, boot).
// ---------------------------------------------------------------------------
static void set_candump_flag(bool enabled) {
  if (s_candump_enabled == enabled)
    return;  // no change — avoid log spam on repeated toggles
  s_candump_enabled = enabled;
  if (enabled) {
    s_candump_start_ms = millis();
    ESP_LOGW(TAG, "CANDUMP debug ENABLED — logging CAN frames (auto-off in %us, or turn off switch)",
             (unsigned) (CANDUMP_AUTO_OFF_MS / 1000));
  } else {
    ESP_LOGW(TAG, "CANDUMP debug DISABLED");
  }
  TopTronic::candump_update_callbacks.call(s_candump_enabled);
}

static void set_find_can_id_flag(bool enabled) {
  if (s_find_can_id_enabled == enabled)
    return;  // no change — avoid log spam on repeated toggles
  s_find_can_id_enabled = enabled;
  if (enabled) {
    s_find_can_id_start_ms = millis();
    ESP_LOGW(TAG, "FIND CAN-ID debug ENABLED — logging 0x42/0x40 frames (auto-off in %us, or turn off switch)",
             (unsigned) (FIND_CAN_ID_AUTO_OFF_MS / 1000));
  } else {
    ESP_LOGW(TAG, "FIND CAN-ID debug DISABLED");
  }
  TopTronic::find_can_id_update_callbacks.call(s_find_can_id_enabled);
}

// Optional per-frame debug logging (canbus.yaml candump / Find can_id blocks).
// Registered exactly ONCE across all hubs (build-wide flags/s_debug_callback_registered);
// called from a dedicated canbus callback, not from parse_frame(), so a frame is
// logged once instead of once per hub. Each feature is an independent flag:
//   CANDUMP      — log every frame as "0x%08X : %02X %02X ..."
//   FIND_CAN_ID  — log only frames whose data[1] is a 0x42 response or 0x40 request
// Both may be active at the same time (candump logs frames; find_can_id
// still emits its WARN lines). Uses the same tags as the old canbus.yaml debug
// blocks (candump / can_id_find).
//
// Emit one candump line ("0x%08X : AA BB CC") for a CAN frame. Rate-limited to
// CANDUMP_MIN_LOG_GAP_MS so a busy bus cannot saturate the API logger socket and
// drop the TCP connection to Home Assistant. Frames skipped by the limiter are
// counted and reported with the next emitted line, so a capture is never silently
// incomplete. Both the receive and the transmit path go through here.
static void log_candump_line_(uint32_t can_id, const std::vector<uint8_t> &data) {
  const uint32_t now = millis();
  if (now - s_last_candump_log_ms < CANDUMP_MIN_LOG_GAP_MS) {
    s_candump_dropped++;
    return;
  }
  s_last_candump_log_ms = now;

  char hex[HEX_PRETTY_BUFFER_SIZE];
  format_hex_pretty_to(hex, data, ' ');
  if (s_candump_dropped > 0) {
    ESP_LOGI(CANDUMP_TAG, "0x%08X : %s (%u frames dropped)", (unsigned) can_id, hex, (unsigned) s_candump_dropped);
    s_candump_dropped = 0;
  } else {
    ESP_LOGI(CANDUMP_TAG, "0x%08X : %s", (unsigned) can_id, hex);
  }
}

// The auto-off check runs HERE on every frame, NOT only in drain_refresh_burst():
// candump turns the loop task into a bottleneck (per-frame string builds +
// logging), so the loop may be starved while frames still arrive. Checking the
// deadline per frame guarantees a debug mode always self-disables under flood.
static void debug_log_frame(const std::vector<uint8_t> &data, uint32_t can_id) {
  const uint32_t now = millis();

  if (s_candump_enabled) {
    if (now - s_candump_start_ms > CANDUMP_AUTO_OFF_MS) {
      set_candump_flag(false);
    } else {
      log_candump_line_(can_id, data);
    }
  }

  if (s_find_can_id_enabled) {
    if (now - s_find_can_id_start_ms > FIND_CAN_ID_AUTO_OFF_MS) {
      set_find_can_id_flag(false);
    } else if (data.size() >= 2) {
      if (data[1] == RESPONSE) {
        // Logged with the toptronic tag at WARN so the message also reaches the
        // "main logs" text sensor via the logger.on_message WARN trigger.
        ESP_LOGW(TAG, "Find can_id: response frame, node 0x%03X (CAN-ID 0x%08X)", (unsigned) ((can_id >> 11) & 0x7FF),
                 (unsigned) can_id);
      } else if (data[1] == GET_REQ) {
        ESP_LOGW(TAG, "Find can_id: request frame, node 0x%03X (CAN-ID 0x%08X)", (unsigned) ((can_id >> 11) & 0x7FF),
                 (unsigned) can_id);
      }
    }
  }
}

// Optional debug logging of OUTGOING request frames (GET/SET) while candump is
// active. The receive callback never sees the frames this node transmits (CAN is
// half-duplex, no local echo), so without this a candump capture would show only
// the device responses ("all outputs") and the "frame requested" would be
// missing. Exporting the TX frames with the same tag and format as the RX path
// makes a capture a complete request → response conversation.
//
// Unlike debug_log_frame() this is NOT subject to the 33 ms RX rate limit:
// outgoing frames are sparse (bounded by the poll/SET rate, a few per second)
// and dropping one would defeat the feature. The auto-off deadline is still
// enforced on every frame.
static void debug_log_tx_frame(const std::vector<uint8_t> &data, uint32_t can_id) {
  if (!s_candump_enabled)
    return;
  const uint32_t now = millis();
  if (now - s_candump_start_ms > CANDUMP_AUTO_OFF_MS) {
    set_candump_flag(false);
    return;
  }
  log_candump_line_(can_id, data);
}

void TopTronic::set_candump_enabled(bool enabled) { set_candump_flag(enabled); }

void TopTronic::set_find_can_id_enabled(bool enabled) { set_find_can_id_flag(enabled); }

bool TopTronic::get_candump_enabled() { return s_candump_enabled; }

bool TopTronic::get_find_can_id_enabled() { return s_find_can_id_enabled; }

void TopTronic::add_candump_update_callback(std::function<void(bool)> &&callback) {
  candump_update_callbacks.add(std::move(callback));
}

void TopTronic::add_find_can_id_update_callback(std::function<void(bool)> &&callback) {
  find_can_id_update_callbacks.add(std::move(callback));
}

#ifdef USE_SWITCH
void TopTronicDebugSwitch::setup() {
  // Mirror the real (build-wide) flag for this switch's feature, both now and on
  // every future change, so the switch state always reflects what the component
  // is actually doing. Each switch registers on its OWN manager, so turning one
  // switch on/off never fans out to the other.
  if (this->mode_ == DebugMode::CANDUMP) {
    this->parent_->add_candump_update_callback([this](bool enabled) { this->publish_state(enabled); });
    this->publish_state(this->parent_->get_candump_enabled());
  } else {
    this->parent_->add_find_can_id_update_callback([this](bool enabled) { this->publish_state(enabled); });
    this->publish_state(this->parent_->get_find_can_id_enabled());
  }
}

void TopTronicDebugSwitch::write_state(bool state) {
  // state=true → enable this switch's debug feature; state=false → disable it.
  // Only the feature selected by mode_ is touched; the other switch is unaffected.
  if (this->mode_ == DebugMode::CANDUMP) {
    this->parent_->set_candump_enabled(state);
  } else {
    this->parent_->set_find_can_id_enabled(state);
  }
  // With assumed_state() the toggle is applied optimistically by HA, but the
  // underlying flag may already be in the requested state (e.g. re-disabling
  // after an auto-off), in which case the setter above emits no state change.
  // Publishing here guarantees the device-side entity always tracks the user's
  // action, so the switch can always be turned back off again.
  this->publish_state(state);
}

void TopTronicDebugSwitch::dump_config() {
  LOG_SWITCH("", "TopTronic Debug Switch", this);
  ESP_LOGCONFIG(TAG, "  Debug mode: %s", this->mode_ == DebugMode::CANDUMP ? "candump" : "find_can_id");
}
#endif

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
      ESP_LOGW(TAG, "Dropping malformed start frame (%zu bytes)", data.size());
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
      ESP_LOGD(TAG, "Dropping start frame with implausible frame count %u (header 0x%02X)", num_remaining, data[0]);
      return;
    }
    if (num_remaining == 1) {
      // A start frame always begins a multi-frame message (total frame count >= 2).
      // total==1 is malformed: remaining_frames would start at 0, so no continuation
      // could ever complete it and the entry would sit as a zombie until the stale
      // sweep evicts it. Drop it like any other implausible header.
      ESP_LOGD(TAG, "Dropping start frame with total frame count 1 (header 0x%02X)", data[0]);
      return;
    }
    if (num_remaining == 0) {
      // Single-frame message: strip the length byte and dispatch directly.
      this->interpret_message_(data.data() + 1, data.size() - 1, can_id, remote_transmission_request);
    } else {
      // Multi-frame message: save the first fragment and wait for the rest.
      uint8_t msg_header = data[1];  // reassembly key shared across all frames of this message
      uint32_t header_key = (device_id << 8) | msg_header;
      char hex[HEX_LOG_BUFFER_SIZE];
      format_hex_to(hex, data);
      ESP_LOGD(TAG, "     - Start of message with id: %d with length %d (Can-ID: 0x%08X, Data: 0x%s)", msg_header,
               num_remaining, (unsigned) can_id, hex);
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
        // by the sweep interval, so all ages here are far below the wrap window.
        uint32_t oldest_key = this->pending_messages_.begin()->first;
        uint32_t oldest_age = now - this->pending_messages_.begin()->second.last_update_ms;
        for (auto &pending_message : this->pending_messages_) {
          if (now - pending_message.second.last_update_ms > oldest_age) {
            oldest_key = pending_message.first;
            oldest_age = now - pending_message.second.last_update_ms;
          }
        }
        ESP_LOGW(TAG, "Pending message buffer full (%zu entries), evicting oldest 0x%08X",
                 this->pending_messages_.size(), (unsigned int) oldest_key);
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
      ESP_LOGW(TAG, "Dropping malformed continuation frame (%zu bytes)", data.size());
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
        char hex[HEX_LOG_BUFFER_SIZE];
        format_hex_to(hex, data);
        ESP_LOGD(TAG, "[IGN] Low-priority frame from node 0x%03X (Can-ID: 0x%08X, Data: 0x%s)", (unsigned) device_id,
                 (unsigned) can_id, hex);
      } else {
        // Continuation for an unknown/expired message (evicted, purged, or the start
        // frame never arrived). Logged at DEBUG so lost reassemblies are visible.
        char hex[HEX_LOG_BUFFER_SIZE];
        format_hex_to(hex, data);
        ESP_LOGD(TAG,
                 "[DROP] Continuation for unknown/expired message (node 0x%03X, header 0x%02X) Can-ID: 0x%08X, "
                 "Data: 0x%s",
                 (unsigned) device_id, msg_header, (unsigned) can_id, hex);
      }
      return;
    }
    PendingMessage &pending = it->second;
    if (pending.remaining_frames == 0) {
      // Duplicate/extra continuation for an already-complete message — discard.
      ESP_LOGD(TAG, "[DROP] Extra continuation for complete message (node 0x%03X, header 0x%02X)", (unsigned) device_id,
               msg_header);
      this->pending_messages_.erase(it);
      return;
    }
    ESP_LOGD(TAG, "     - Part of message with id: %d with remaining length %d", msg_header,
             pending.remaining_frames - 1);
    pending.data.insert(pending.data.end(), data.begin() + 1, data.end());
    pending.last_update_ms = now;
    pending.remaining_frames--;

    if (pending.remaining_frames == 0) {
      const size_t msg_len = pending.data.size();
      if (msg_len < MIN_MESSAGE_LEN + 2) {
        ESP_LOGW(TAG, "Reassembled message too short for CRC (%zu bytes)", msg_len);
        this->pending_messages_.erase(it);
        return;
      }

      const uint8_t *msg = pending.data.data();
      uint16_t received_crc = (msg[msg_len - 2] << 8) | msg[msg_len - 1];
      uint16_t computed_crc = crc16(msg, msg_len - 2, 0x600D, 0x8408);

      if (received_crc != computed_crc) {
        ESP_LOGW(TAG, "CRC check failed! Recv: 0x%04X, Comp: 0x%04X", received_crc, computed_crc);
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
    ESP_LOGD(TAG, "Message too short (%u bytes), ignoring", (unsigned) len);
    return;
  }

  // Ignore outgoing GET/SET requests that we echoed ourselves — nothing to update.
  if (data[0] == GET_REQ) {
    char hex[HEX_LOG_BUFFER_SIZE];
    format_hex_to(hex, data, len);
    ESP_LOGD(TAG, "[GET] Can-ID: 0x%08X, Data: 0x%s", (unsigned int) can_id, hex);
    return;
  }

  if (data[0] == SET_REQ) {
    char hex[HEX_LOG_BUFFER_SIZE];
    format_hex_to(hex, data, len);
    ESP_LOGD(TAG, "[SET] Can-ID: 0x%08X, Data: 0x%s", (unsigned int) can_id, hex);
    return;
  }

  bool is_response = (data[0] == RESPONSE || data[0] == RESPONSE_EXT);
  if (!is_response) {
    char hex[HEX_LOG_BUFFER_SIZE];
    format_hex_to(hex, data, len);
    ESP_LOGD(TAG, "[UNK] Can-ID: 0x%08X, Data: 0x%s", (unsigned int) can_id, hex);
    return;
  }
  // 0x56 inserts 2 header bytes (0x80 0x00) between the datapoint and the value.
  const size_t value_off = (data[0] == RESPONSE_EXT) ? 7 : 5;
  if (len < value_off) {
    ESP_LOGD(TAG, "Response without value bytes (%u bytes), ignoring", (unsigned) len);
    return;
  }

  // The sender node ID sits in bits 21-11 of the CAN ID.
  uint32_t rx_device_id = (can_id >> 11) & 0x7FF;

  auto device_it = this->devices_.find(rx_device_id);
  if (device_it == this->devices_.end()) {
    // Message from a device we have no entities registered for. Logged at DEBUG
    // so unknown node ids (e.g. a misconfigured device_type tag) are visible
    // instead of silently dropping every frame from that device.
    ESP_LOGD(TAG, "[DROP] No device registered for node id 0x%03X, cmd=0x%02X fg=%u fn=%u dp_hi=0x%02X dp_lo=0x%02X",
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
    ESP_LOGD(TAG, "[DROP] No sensor for key 0x%08X on node 0x%03X (fg=%u fn=%u dp=%u)", (unsigned) id,
             (unsigned) rx_device_id, data[1], data[2], (unsigned) datapoint);
    return;
  }
  TopTronicDatapoint *sensor_base = sensor_it->second;

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
    ESP_LOGW(TAG, "Response for %s truncated (%u of %u value bytes), ignoring",
             LOG_STR_ARG(sensor_base->get_log_name()), (unsigned) value_len, (unsigned) required_value_len);
    return;
  }

  // A RESPONSE with a complete value for this datapoint arrived, so the
  // cold-cache write guard (reject_writes_before_read_) may let SET requests
  // through from now on.
  this->read_ok_ids_.insert(id);

  // This sensor was answered — drop it from the refresh-retry queue so the burst
  // does not re-poll it. The deque is small (self-hub sensors) and the burst
  // drain is O(1) at the front, so a linear erase here is cheap and only runs
  // when a response for a pending refresh sensor actually arrives.
  if (!this->pending_refresh_.empty()) {
    for (auto rit = this->pending_refresh_.begin(); rit != this->pending_refresh_.end(); ++rit) {
      if (rit->sensor == sensor_base) {
        this->pending_refresh_.erase(rit);
        // Burst progress: a pending GET was answered. Count it for the
        // completion log and refresh the stall-watchdog timestamp.
        if (this->burst_in_progress_) {
          this->burst_answered_++;
          this->last_burst_progress_ms_ = millis();
        }
        break;
      }
    }
  }

  // Downcast to the concrete type and publish the decoded value.
  // data[value_off..] contains the raw value bytes.
#ifdef USE_SENSOR
  if (sensor_base->type() == SENSOR) {
    auto *sensor = static_cast<TopTronicSensor *>(sensor_base);
    float value = sensor->parse_value(data + value_off, required_value_len);
    sensor->publish_state(value);
    log_response_frame(data, len, can_id, sensor->get_log_name());
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (sensor_base->type() == TEXTSENSOR) {
    auto *sensor = static_cast<TopTronicTextSensor *>(sensor_base);
    std::string value = sensor->parse_value(data + value_off, value_len);
    sensor->publish_state(value);
    log_response_frame(data, len, can_id, sensor->get_log_name());
  }
#endif
}

}  // namespace esphome::toptronic

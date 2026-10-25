#include "toptronic.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

namespace esphome::toptronic {

ESPHOME_LOG_TAG(TAG, "toptronic");

static constexpr uint8_t RESPONSE = 0x42;
// Extended RESPONSE for larger values (cleaning and maintenance counters): the 0x42 layout with two extra
// bytes (0x80 0x00) between the datapoint and the value
static constexpr uint8_t RESPONSE_EXT = 0x56;
static constexpr uint8_t GET_REQ = 0x40;
static constexpr uint8_t SET_REQ = 0x46;
// A CAN frame holds 8 bytes: a start frame spends 2 on headers, a continuation frame 1.
// Continuation CAN ids clear bits 28-22.
static constexpr size_t MAX_FIRST_FRAME_PAYLOAD = 6;
static constexpr uint32_t CONTINUATION_ID_MASK = 0x003FFFFF;
static constexpr uint8_t START_OF_MESSAGE_ID = 0x1F;
static constexpr size_t MIN_MESSAGE_LEN = 5;  // cmd | fg | fn | dp_hi | dp_lo
static constexpr size_t CRC_LEN = 2;
static constexpr size_t CAN_FRAME_LEN = 8;
static constexpr uint16_t GATEWAY_DEVICE_TYPE = 1153;  // GW
// Device types are 64 apart, so the low 6 bits of a device id are its address
static constexpr uint16_t DEVICE_ADDR_MASK = 0x3F;
// refresh_all() starts one hub's refresh this long after the previous one
static constexpr uint32_t REFRESH_STAGGER_MS = 15000;
// A refresh with no GET sent or answered for this long plus the retry interval is abandoned
static constexpr uint32_t REFRESH_STALL_TIMEOUT_MS = 5000;
static constexpr uint32_t BOOT_REFRESH_TIMEOUT_ID = 0;
static constexpr uint32_t REFRESH_ALL_TIMEOUT_ID = 1;

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

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
StaticVector<TopTronic *, TopTronic::MAX_HUBS> TopTronic::hubs;
bool TopTronic::boot_refresh_done{false};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

TopTronic::TopTronic(canbus::Canbus *canbus, uint16_t device_id) : canbus_(canbus), device_id_(device_id) {
  hubs.push_back(this);
  canbus->add_callback([this](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
    // A remote frame has no payload; the bytes passed with it are left over from an earlier frame
    if (rtr || this->paused_ || !extended_id || sender_id(can_id) != this->device_id_)
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
  ESP_LOGD_HEX(TAG, data.data(), len, MAX_SET_REQUEST_LEN, "[SET] %s: %f, Data: %s", LOG_STR_ARG(this->get_log_name()),
               value);
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
void TopTronicNumber::control(float value) { this->send_set_(this->multiplier_ * value, this->type_); }
#endif
#ifdef USE_TEXT_SENSOR
std::string TopTronicTextSensor::parse_value(const uint8_t *data, size_t len) {
  const uint8_t raw = bytes_to_number<uint8_t>(data, len);
  auto it = this->to_text_.find(raw);
  if (it == this->to_text_.end()) {
    ESP_LOGW(TAG, "Unknown text sensor value: %u", raw);
    return "";
  }
  return it->second;
}
#endif
#ifdef USE_SELECT
void TopTronicSelect::control(const std::string &text) {
  auto it = this->to_value_.find(text);
  if (it == this->to_value_.end()) {
    ESP_LOGW(TAG, "[SET] Unknown option '%s', ignoring", text.c_str());
    return;
  }
  this->send_set_(static_cast<float>(it->second), this->type_);
}
#endif

void TopTronic::add_sensor(TopTronicBase *sensor) { this->device_.get_sensors()[sensor->get_id()] = sensor; }
void TopTronic::add_input(TopTronicBase *input) { this->device_.get_inputs()[input->get_id()] = input; }

void TopTronic::register_sensor_callbacks() {
  const uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | (this->device_id_ & DEVICE_ADDR_MASK), this->device_id_);
  for (const auto &s : this->device_.get_sensors()) {
    auto *sensor = s.second;
    sensor->cache_request_data();
    sensor->add_on_update_callback([this, sensor, can_id]() -> void {
      if (this->paused_)
        return;
      const auto &data = sensor->get_request_data();
      this->canbus_->send_data(can_id, true, false, data.data(), data.size());
      ESP_LOGD_HEX(TAG, data.data(), data.size(), GET_REQUEST_LEN, "[GET] dev 0x%04X Data: %s", this->device_id_);
    });
  }
}

// A request longer than one frame goes out as a start frame plus continuation frames, the same layout
// parse_frame_() reassembles:
//   start frame (msg_id 0x1F): [total frames << 3 | 0x01, tag, payload[0..5]]
//   continuation frames:       [tag, next 7 payload bytes]
// The payload ends with a CRC-16 (poly 0x1021, init 0xB006, reflected) sent big endian.
canbus::Error TopTronic::send_frames(canbus::Canbus *canbus, uint32_t can_id, const uint8_t *data, size_t len) {
  if (len <= CAN_FRAME_LEN)
    return canbus->send_data(can_id, true, false, data, len);
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
  uint8_t frame[CAN_FRAME_LEN];
  frame[0] = static_cast<uint8_t>(((num_cont + 1) << 3) | 0x01);
  frame[1] = msg_header;
  std::memcpy(frame + 2, msg, first_chunk);
  canbus::Error result = canbus->send_data(can_id, true, false, frame, first_chunk + 2);
  const uint32_t cont_id = can_id & CONTINUATION_ID_MASK;
  for (size_t offset = first_chunk; offset < msg_len;) {
    const size_t chunk = std::min(MAX_CONT_FRAME_PAYLOAD, msg_len - offset);
    frame[0] = msg_header;
    std::memcpy(frame + 1, msg + offset, chunk);
    offset += chunk;
    const canbus::Error err = canbus->send_data(cont_id, true, false, frame, chunk + 1);
    if (result == canbus::ERROR_OK)
      result = err;
  }
  if (result != canbus::ERROR_OK) {
    ESP_LOGW(TAG, "[SET] Multi-frame send failed (canbus error %u, msg_header=0x%02X)", (unsigned) result, msg_header);
  } else {
    ESP_LOGD(TAG, "[SET] Sent %u CAN frames (msg_header=0x%02X, payload=%zu bytes)", 1 + num_cont, msg_header, len);
  }
  return result;
}

void TopTronic::register_input_callbacks() {
  const uint32_t can_id = build_can_id(GATEWAY_DEVICE_TYPE | (this->device_id_ & DEVICE_ADDR_MASK), this->device_id_);
  for (const auto &i : this->device_.get_inputs()) {
    auto *input = i.second;
    // Write-only datapoints have no sensor and are never held back by the read guard
    auto *sensor = this->get_sensor_(input->get_id());
    input->add_on_set_callback([this, can_id, input, sensor, last_write_ms = uint32_t{0}, written = false](
                                   const uint8_t *data, size_t len) mutable -> void {
      const uint32_t now = App.get_loop_component_start_time();
      if (written && this->write_min_interval_ms_ != 0 && now - last_write_ms < this->write_min_interval_ms_) {
        ESP_LOGW(TAG, "[SET] %s rate limited (min %" PRIu32 " ms between writes)", LOG_STR_ARG(input->get_log_name()),
                 this->write_min_interval_ms_);
        return;
      }
      if (this->reject_writes_before_read_ && sensor != nullptr && !sensor->was_read()) {
        ESP_LOGW(TAG, "[SET] %s rejected, datapoint not read since boot", LOG_STR_ARG(input->get_log_name()));
        return;
      }
      // Only a write the bus accepted starts the interval, so a failed one can be retried at once
      const canbus::Error err = send_frames(this->canbus_, can_id, data, len);
      if (err != canbus::ERROR_OK) {
        ESP_LOGW(TAG, "[SET] %s failed (canbus error %u)", LOG_STR_ARG(input->get_log_name()), (unsigned) err);
        return;
      }
      last_write_ms = now;
      written = true;
    });
  }
}

void TopTronic::update_all() {
  if (this->refresh_in_progress_) {
    this->refresh_requested_ = true;
    ESP_LOGD(TAG, "Hub 0x%04X refresh in progress (%zu pending), running again after it", (unsigned) this->device_id_,
             this->refresh_queue_.size());
    return;
  }
  this->refresh_queue_.clear();
  for (const auto &s : this->device_.get_sensors())
    this->refresh_queue_.push_back(RefreshEntry{s.second, 0, 0});
  this->refresh_in_progress_ = true;
  this->refresh_queued_ = static_cast<uint16_t>(this->refresh_queue_.size());
  this->refresh_answered_ = 0;
  this->refresh_dropped_ = 0;
  this->last_refresh_progress_ms_ = App.get_loop_component_start_time();
  ESP_LOGI(TAG, "Hub 0x%04X refresh requested (%u sensors)%s", (unsigned) this->device_id_,
           (unsigned) this->refresh_queued_, this->paused_ ? LOG_STR_LITERAL(", paused") : LOG_STR_LITERAL(""));
  this->enable_loop();
}

void TopTronic::refresh_all() {
  size_t sensors = 0;
  for (TopTronic *hub : hubs)
    sensors += hub->device_.get_sensors().size();
  ESP_LOGI(TAG, "Refreshing %zu hub(s), %zu sensors, %" PRIu32 " s apart", hubs.size(), sensors,
           REFRESH_STAGGER_MS / 1000);
  uint32_t offset = 0;
  for (TopTronic *hub : hubs) {
    if (offset == 0) {
      hub->update_all();
    } else {
      hub->set_timeout(REFRESH_ALL_TIMEOUT_ID, offset, [hub]() { hub->update_all(); });
    }
    offset += REFRESH_STAGGER_MS;
  }
}

// Sends the GET at the head of the queue once the spacing has passed and, for a retry, once the retry interval
// has passed. A sent entry moves to the back, so the head is always the one sent longest ago.
void TopTronic::drain_refresh_() {
  if (!this->refresh_in_progress_ || this->paused_)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t spacing = (this->refresh_gap_ms_ + this->max_refresh_per_loop_ - 1) / this->max_refresh_per_loop_;
  if (!this->refresh_queue_.empty() && now - this->last_refresh_send_ms_ >= spacing) {
    RefreshEntry entry = this->refresh_queue_.front();
    if (entry.attempts == 0 || now - entry.last_send_ms >= this->refresh_retry_interval_ms_) {
      std::copy(this->refresh_queue_.begin() + 1, this->refresh_queue_.end(), this->refresh_queue_.begin());
      this->refresh_queue_.pop_back();
      if (entry.attempts > this->max_refresh_retries_) {
        // The regular poll still covers a sensor that never answered
        this->refresh_dropped_++;
        ESP_LOGD(TAG, "[GET] %s unanswered after %u tries", LOG_STR_ARG(entry.sensor->get_log_name()), entry.attempts);
      } else {
        if (entry.attempts > 0) {
          ESP_LOGD(TAG, "[GET] %s retry %u/%u", LOG_STR_ARG(entry.sensor->get_log_name()), entry.attempts,
                   this->max_refresh_retries_);
        }
        entry.last_send_ms = now;
        entry.attempts++;
        this->refresh_queue_.push_back(entry);
        entry.sensor->update();
      }
      this->last_refresh_send_ms_ = now;
      this->last_refresh_progress_ms_ = now;
    }
  }
  if (this->refresh_queue_.empty()) {
    this->finish_refresh_(false);
  } else if (now - this->last_refresh_progress_ms_ >= REFRESH_STALL_TIMEOUT_MS + this->refresh_retry_interval_ms_) {
    ESP_LOGW(TAG, "Hub 0x%04X refresh stalled with %zu sensors pending, abandoning it", (unsigned) this->device_id_,
             this->refresh_queue_.size());
    this->refresh_dropped_ += this->refresh_queue_.size();
    this->refresh_queue_.clear();
    this->finish_refresh_(true);
  }
}

void TopTronic::finish_refresh_(bool stalled) {
  this->refresh_in_progress_ = false;
  ESP_LOGI(TAG, "Hub 0x%04X refresh finished%s: %u queued, %u answered, %u dropped", (unsigned) this->device_id_,
           stalled ? LOG_STR_LITERAL(" (stalled)") : LOG_STR_LITERAL(""), (unsigned) this->refresh_queued_,
           (unsigned) this->refresh_answered_, (unsigned) this->refresh_dropped_);
  if (this->refresh_requested_) {
    this->refresh_requested_ = false;
    this->update_all();
  }
}

void TopTronic::remove_refresh_entry_(TopTronicBase *sensor) {
  auto *it = std::find_if(this->refresh_queue_.begin(), this->refresh_queue_.end(),
                          [sensor](const RefreshEntry &entry) { return entry.sensor == sensor; });
  if (it == this->refresh_queue_.end())
    return;
  std::copy(it + 1, this->refresh_queue_.end(), it);
  this->refresh_queue_.pop_back();
  this->refresh_answered_++;
  this->last_refresh_progress_ms_ = App.get_loop_component_start_time();
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

void TopTronic::pause() {
  this->paused_ = true;
  ESP_LOGI(TAG, "Hub 0x%04X paused", (unsigned) this->device_id_);
}

void TopTronic::resume() {
  this->paused_ = false;
  // Time spent paused does not count towards the refresh stall timeout
  this->last_refresh_progress_ms_ = App.get_loop_component_start_time();
  ESP_LOGI(TAG, "Hub 0x%04X resumed", (unsigned) this->device_id_);
}

void TopTronic::setup() {
  // Wire the callbacks here, after every platform's add_sensor()/add_input() has run.
  this->link_inputs_();
  this->register_sensor_callbacks();
  this->register_input_callbacks();
  this->refresh_queue_.init(this->device_.get_sensors().size());
  if (this->boot_refresh_delay_ms_ != 0) {
    // Every hub arms its own delay; the first to expire refreshes all of them
    this->set_timeout(BOOT_REFRESH_TIMEOUT_ID, this->boot_refresh_delay_ms_, [this]() {
      if (!boot_refresh_done) {
        boot_refresh_done = true;
        this->refresh_all();
      }
    });
  }
#ifdef USE_OTA_STATE_LISTENER
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
  ESP_LOGI(TAG, "Hub 0x%04X registered (%zu sensors, %zu inputs)", (unsigned) this->device_id_,
           this->device_.get_sensors().size(), this->device_.get_inputs().size());
}

void TopTronic::loop() {
  this->drain_refresh_();
  if (!this->refresh_in_progress_)
    this->disable_loop();
}

#ifdef USE_OTA_STATE_LISTENER
// Stop bus traffic while an OTA update runs so it does not compete with the update
void TopTronic::on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *component) {
  if (state == ota::OTA_STARTED) {
    this->pause();
  } else if (state == ota::OTA_COMPLETED || state == ota::OTA_ABORT || state == ota::OTA_ERROR) {
    this->resume();
  }
}
#endif

void TopTronic::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TopTronic:\n"
                "  Device: 0x%04X (type 0x%04X, address 0x%02X)\n"
                "  Sensors: %zu, inputs: %zu\n"
                "  Write safety: min SET interval %" PRIu32 " ms, reject writes before read: %s\n"
                "  Refresh: boot delay %" PRIu32 " ms, %u GETs per %" PRIu32 " ms, %u retries after %" PRIu32 " ms",
                (unsigned) this->device_id_, (unsigned) (this->device_id_ & ~DEVICE_ADDR_MASK),
                (unsigned) (this->device_id_ & DEVICE_ADDR_MASK), this->device_.get_sensors().size(),
                this->device_.get_inputs().size(), this->write_min_interval_ms_,
                YESNO(this->reject_writes_before_read_), this->boot_refresh_delay_ms_, this->max_refresh_per_loop_,
                this->refresh_gap_ms_, this->max_refresh_retries_, this->refresh_retry_interval_ms_);
}

bool TopTronic::parse_frame_(const uint8_t *data, size_t size, uint32_t can_id, const uint8_t **msg, size_t *len) {
  if (size < 2) {
    ESP_LOGW(TAG, "Dropping malformed frame (%zu bytes)", size);
    return false;
  }
  const uint32_t now = App.get_loop_component_start_time();
  const bool start = (can_id >> 24) == START_OF_MESSAGE_ID;
  // Start frame: [0] total frame count << 3, [1] tag, payload. Continuation: [0] tag, payload.
  const uint8_t tag = start ? data[1] : data[0];
  const auto slots = std::span<Slot>(this->slots_.data(), this->max_pending_messages_);
  Slot *slot = nullptr;
  for (auto &s : slots) {
    if (s.tag == tag && s.is_live(now, this->max_pending_age_ms_)) {
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
    // A message needs at least two frames; U32/S32 values use 2 and S64 uses 3, so a count above the
    // configured limit is a corrupted header
    if (total_frames == 1 || total_frames > this->max_frames_per_message_) {
      ESP_LOGD(TAG, "Dropping start frame with implausible frame count %u (header 0x%02X)", total_frames, data[0]);
      return false;
    }
    if (slot == nullptr) {
      // Take a free or stale slot, else evict the oldest
      slot = &slots[0];
      for (auto &s : slots) {
        if (!s.is_live(now, this->max_pending_age_ms_)) {
          slot = &s;
          break;
        }
        if (now - s.last_update_ms > now - slot->last_update_ms)
          slot = &s;
      }
      if (slot->remaining != 0) {
        ESP_LOGD(TAG, "Expiring pending message 0x%02X (%u bytes, %u frames remaining)", slot->tag, slot->len,
                 slot->remaining);
      }
    }
    ESP_LOGD_HEX(TAG, data, size, CAN_FRAME_LEN, "Start of message 0x%02X, %u frames (Data: %s)", tag, total_frames);
    slot->tag = tag;
    slot->len = 0;
    slot->remaining = total_frames;  // the start frame is consumed below like a continuation
  } else if (slot == nullptr) {
    // msg_id 0 marks a standalone low priority broadcast (presence or ACK), not a continuation
    if ((can_id >> 24) == 0) {
      ESP_LOGD_HEX(TAG, data, size, CAN_FRAME_LEN, "[IGN] Low priority frame (Can-ID: 0x%08" PRIX32 ", Data: %s)",
                   can_id);
    } else {
      ESP_LOGD_HEX(TAG, data, size, CAN_FRAME_LEN,
                   "[DROP] Continuation for unknown or expired message 0x%02X (Can-ID: 0x%08" PRIX32 ", Data: %s)", tag,
                   can_id);
    }
    return false;
  } else {
    ESP_LOGD(TAG, "Part of message 0x%02X, %u frames remaining", tag, slot->remaining - 1);
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
  if (buf_len < MIN_MESSAGE_LEN + CRC_LEN) {
    ESP_LOGW(TAG, "Reassembled message too short for CRC (%zu bytes)", buf_len);
    return false;
  }
  const uint16_t received_crc = encode_uint16(buf[buf_len - 2], buf[buf_len - 1]);
  const uint16_t computed_crc = crc16(buf, buf_len - CRC_LEN, 0x600D, 0x8408);
  if (received_crc != computed_crc) {
    ESP_LOGW(TAG, "CRC check failed (received 0x%04X, computed 0x%04X)", received_crc, computed_crc);
    return false;
  }
  *msg = buf;
  *len = buf_len - CRC_LEN;
  return true;
}

// [0] cmd (0x40 GET, 0x46 SET, 0x42/0x56 RESPONSE), [1] fg, [2] fn,
// [3..4] datapoint, [5..] value (0x56 value starts at [7]).
void TopTronic::interpret_message_(const uint8_t *data, size_t len) {
  if (len < MIN_MESSAGE_LEN) {
    ESP_LOGD(TAG, "Message too short (%zu bytes), ignoring", len);
    return;
  }
  // GET and SET requests sent by the device carry nothing to publish
  if (data[0] == GET_REQ || data[0] == SET_REQ) {
    ESP_LOGD_HEX(TAG, data, len, MAX_MESSAGE_LEN, "%s dev 0x%04X Data: %s",
                 data[0] == GET_REQ ? LOG_STR_LITERAL("[GET]") : LOG_STR_LITERAL("[SET]"), this->device_id_);
    return;
  }
  if (data[0] != RESPONSE && data[0] != RESPONSE_EXT) {
    ESP_LOGD_HEX(TAG, data, len, MAX_MESSAGE_LEN, "[UNK] dev 0x%04X Data: %s", this->device_id_);
    return;
  }
  const size_t value_off = data[0] == RESPONSE_EXT ? 7 : MIN_MESSAGE_LEN;
  if (len < value_off) {
    ESP_LOGD(TAG, "Response without value bytes (%zu bytes), ignoring", len);
    return;
  }
  const uint16_t datapoint = encode_uint16(data[3], data[4]);
  uint32_t id = data[1] + (data[2] << 8) + (static_cast<uint32_t>(datapoint) << 16);
  auto &sensors = this->device_.get_sensors();
  auto sensor_it = sensors.find(id);
  if (sensor_it == sensors.end()) {
    // Usually a preset whose fg/fn/dp differ from what the device sends
    ESP_LOGD(TAG, "[DROP] No sensor for fg=%u fn=%u dp=%u", data[1], data[2], datapoint);
    return;
  }
  TopTronicBase *sensor_base = sensor_it->second;
  const size_t value_len = len - value_off;
  size_t required_value_len = 1;
#ifdef USE_SENSOR
  if (sensor_base->type() == SENSOR)
    required_value_len = static_cast<TopTronicSensor *>(sensor_base)->value_width();
#endif
  if (value_len < required_value_len) {
    ESP_LOGW(TAG, "Response for %s truncated (%zu of %zu value bytes), ignoring",
             LOG_STR_ARG(sensor_base->get_log_name()), value_len, required_value_len);
    return;
  }
  sensor_base->mark_read();
  if (this->refresh_in_progress_)
    this->remove_refresh_entry_(sensor_base);
  ESP_LOGD_HEX(TAG, data, len, MAX_MESSAGE_LEN, "[RES] %s Data: %s", LOG_STR_ARG(sensor_base->get_log_name()));
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

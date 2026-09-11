#include "remote_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote_base";

/* RemoteReceiveData */

bool RemoteReceiveData::peek_mark(uint32_t length, uint32_t offset) const {
  if (!this->is_valid(offset))
    return false;
  const int32_t value = this->peek(offset);
  const int32_t lo = this->lower_bound_(length);
  const int32_t hi = this->upper_bound_(length);
  return value >= 0 && lo <= value && value <= hi;
}

bool RemoteReceiveData::peek_mark_at_least(uint32_t length, uint32_t offset) const {
  if (!this->is_valid(offset))
    return false;
  const int32_t value = this->peek(offset);
  const int32_t lo = this->lower_bound_(length);
  return value >= 0 && lo <= value;
}

bool RemoteReceiveData::peek_mark_at_most(uint32_t length, uint32_t offset) const {
  if (!this->is_valid(offset))
    return false;
  const int32_t value = this->peek(offset);
  const int32_t hi = this->upper_bound_(length);
  return value >= 0 && value <= hi;
}

bool RemoteReceiveData::peek_space(uint32_t length, uint32_t offset) const {
  if (!this->is_valid(offset))
    return false;
  const int32_t value = this->peek(offset);
  const int32_t lo = this->lower_bound_(length);
  const int32_t hi = this->upper_bound_(length);
  return value <= 0 && lo <= -value && -value <= hi;
}

bool RemoteReceiveData::peek_space_at_least(uint32_t length, uint32_t offset) const {
  if (!this->is_valid(offset))
    return false;
  const int32_t value = this->peek(offset);
  const int32_t lo = this->lower_bound_(length);
  return value <= 0 && lo <= -value;
}

bool RemoteReceiveData::peek_space_at_most(uint32_t length, uint32_t offset) const {
  if (!this->is_valid(offset))
    return false;
  const int32_t value = this->peek(offset);
  const int32_t hi = this->upper_bound_(length);
  return value <= 0 && -value <= hi;
}

bool RemoteReceiveData::expect_mark(uint32_t length) {
  if (!this->peek_mark(length))
    return false;
  this->advance();
  return true;
}

bool RemoteReceiveData::expect_space(uint32_t length) {
  if (!this->peek_space(length))
    return false;
  this->advance();
  return true;
}

bool RemoteReceiveData::expect_item(uint32_t mark, uint32_t space) {
  if (!this->peek_item(mark, space))
    return false;
  this->advance(2);
  return true;
}

bool RemoteReceiveData::expect_pulse_with_gap(uint32_t mark, uint32_t space) {
  if (!this->peek_space_at_least(space, 1) || !this->peek_mark(mark))
    return false;
  this->advance(2);
  return true;
}

/* RemoteReceiverBinarySensorBase */

bool RemoteReceiverBinarySensorBase::on_receive(RemoteReceiveData src) {
  if (!this->matches(src))
    return false;
  this->publish_state(true);
  yield();
  this->publish_state(false);
  return true;
}

/* RemoteReceiverBase */

// Slots are counted at code generation; a registration from C++ setup() has none
#ifdef REMOTE_BASE_LISTENER_COUNT
void RemoteReceiverBase::register_listener(RemoteReceiverListener *listener) {
  if (this->listeners_.size() == REMOTE_BASE_LISTENER_COUNT) {
    ESP_LOGE(TAG, "No %s slot: register it from to_code() with remote_base.add_%s", LOG_STR_LITERAL("listener"),
             LOG_STR_LITERAL("listener"));
    return;
  }
  this->listeners_.push_back(listener);
}
#endif

#ifdef REMOTE_BASE_DUMPER_COUNT
void RemoteReceiverBase::register_dumper(RemoteReceiverDumperBase *dumper) {
  if (dumper->is_secondary()) {
    this->secondary_dumper_ = dumper;
    return;
  }
  if (this->dumpers_.size() == REMOTE_BASE_DUMPER_COUNT) {
    ESP_LOGE(TAG, "No %s slot: register it from to_code() with remote_base.add_%s", LOG_STR_LITERAL("dumper"),
             LOG_STR_LITERAL("dumper"));
    return;
  }
  this->dumpers_.push_back(dumper);
}
#endif

void RemoteReceiverBase::call_listeners_dumpers_() {
#ifdef REMOTE_BASE_LISTENER_COUNT
  for (auto *listener : this->listeners_)
    listener->on_receive(RemoteReceiveData(this->temp_, this->tolerance_, this->tolerance_mode_));
#endif
#ifdef REMOTE_BASE_DUMPER_COUNT
  bool success = false;
  for (auto *dumper : this->dumpers_) {
    if (dumper->dump(RemoteReceiveData(this->temp_, this->tolerance_, this->tolerance_mode_)))
      success = true;
  }
  if (!success && this->secondary_dumper_ != nullptr)
    this->secondary_dumper_->dump(RemoteReceiveData(this->temp_, this->tolerance_, this->tolerance_mode_));
#endif
}

void RemoteReceiverBinarySensorBase::dump_config() { LOG_BINARY_SENSOR("", "Remote Receiver Binary Sensor", this); }

/* RemoteTransmitData */

void RemoteTransmitData::set_data_from_packed_sint32(const uint8_t *data, size_t len, size_t count) {
  this->data_.clear();
  this->data_.reserve(count);

  while (len > 0) {
    // Parse varint (inline, no dependency on api component)
    uint32_t raw = 0;
    uint32_t shift = 0;
    uint32_t consumed = 0;
    for (; consumed < len && consumed < 5; consumed++) {
      uint8_t byte = data[consumed];
      raw |= (byte & 0x7F) << shift;
      if ((byte & 0x80) == 0) {
        consumed++;
        break;
      }
      shift += 7;
    }
    if (consumed == 0)
      break;  // Parse error

    // Zigzag decode: (n >> 1) ^ -(n & 1)
    int32_t decoded = static_cast<int32_t>((raw >> 1) ^ (~(raw & 1) + 1));
    this->data_.push_back(decoded);
    data += consumed;
    len -= consumed;
  }
}

bool RemoteTransmitData::set_data_from_base64url(const std::string &base64url) {
  return base64_decode_int32_vector(base64url, this->data_);
}

/* RemoteTransmitterBase */

void RemoteTransmitterBase::send_(uint32_t send_times, uint32_t send_wait) {
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  const auto &vec = this->temp_.get_data();
  char buffer[256];
  size_t pos = buf_append_printf(buffer, sizeof(buffer), 0,
                                 "Sending times=%" PRIu32 " wait=%" PRIu32 "ms: ", send_times, send_wait);

  for (size_t i = 0; i < vec.size(); i++) {
    const int32_t value = vec[i];
    size_t prev_pos = pos;

    if (i + 1 < vec.size()) {
      pos = buf_append_printf(buffer, sizeof(buffer), pos, "%" PRId32 ", ", value);
    } else {
      pos = buf_append_printf(buffer, sizeof(buffer), pos, "%" PRId32, value);
    }

    if (pos >= sizeof(buffer) - 1) {
      // buffer full, flush and continue
      buffer[prev_pos] = '\0';
      ESP_LOGVV(TAG, "%s", buffer);
      if (i + 1 < vec.size()) {
        pos = buf_append_printf(buffer, sizeof(buffer), 0, "  %" PRId32 ", ", value);
      } else {
        pos = buf_append_printf(buffer, sizeof(buffer), 0, "  %" PRId32, value);
      }
    }
  }
  if (pos != 0) {
    ESP_LOGVV(TAG, "%s", buffer);
  }
#endif
  this->send_internal(send_times, send_wait);
}
}  // namespace esphome::remote_base

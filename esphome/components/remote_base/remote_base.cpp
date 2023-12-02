#include "remote_base.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote_base";

#ifdef USE_ESP32
RemoteRMTChannel::RemoteRMTChannel(uint8_t mem_block_num) : mem_block_num_(mem_block_num) {
  static rmt_channel_t next_rmt_channel = RMT_CHANNEL_0;
  this->channel_ = next_rmt_channel;
  next_rmt_channel = rmt_channel_t(int(next_rmt_channel) + mem_block_num);
}

void RemoteRMTChannel::config_rmt(rmt_config_t &rmt) {
  if (rmt_channel_t(int(this->channel_) + this->mem_block_num_) >= RMT_CHANNEL_MAX) {
    this->mem_block_num_ = int(RMT_CHANNEL_MAX) - int(this->channel_);
    ESP_LOGW(TAG, "Not enough RMT memory blocks available, reduced to %i blocks.", this->mem_block_num_);
  }
  rmt.channel = this->channel_;
  rmt.clk_div = this->clock_divider_;
  rmt.mem_block_num = this->mem_block_num_;
}
#endif

/* RemoteReceiveData */

bool RemoteReceiveData::peek_mark(uint32_t length, uint32_t offset) const {
  if (!this->is_valid(offset))
    return false;
  const int32_t value = this->peek(offset);
  const int32_t lo = this->lower_bound_(length);
  const int32_t hi = this->upper_bound_(length);
  return value >= 0 && lo <= value && value <= hi;
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

void RemoteReceiverBase::register_dumper(RemoteReceiverDumperBase *dumper) {
  if (dumper->is_secondary()) {
    this->secondary_dumpers_.push_back(dumper);
  } else {
    this->dumpers_.push_back(dumper);
  }
}

void RemoteReceiverBase::call_listeners_() {
  for (auto *listener : this->listeners_)
    listener->on_receive(RemoteReceiveData(this->temp_, this->tolerance_));
}

void RemoteReceiverBase::call_dumpers_() {
  bool success = false;
  for (auto *dumper : this->dumpers_) {
    if (dumper->dump(RemoteReceiveData(this->temp_, this->tolerance_)))
      success = true;
  }
  if (!success) {
    for (auto *dumper : this->secondary_dumpers_)
      dumper->dump(RemoteReceiveData(this->temp_, this->tolerance_));
  }
}

void RemoteReceiverBinarySensorBase::dump_config() { LOG_BINARY_SENSOR("", "Remote Receiver Binary Sensor", this); }

void RemoteTransmitterBase::send_(uint32_t send_times, uint32_t send_wait) {
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  const auto &vec = this->temp_.get_data();
  char buffer[256];
  uint32_t buffer_offset = 0;
  buffer_offset += sprintf(buffer, "Sending times=%" PRIu32 " wait=%" PRIu32 "ms: ", send_times, send_wait);

  for (size_t i = 0; i < vec.size(); i++) {
    const int32_t value = vec[i];
    const uint32_t remaining_length = sizeof(buffer) - buffer_offset;
    int written;

    if (i + 1 < vec.size()) {
      written = snprintf(buffer + buffer_offset, remaining_length, "%" PRId32 ", ", value);
    } else {
      written = snprintf(buffer + buffer_offset, remaining_length, "%" PRId32, value);
    }

    if (written < 0 || written >= int(remaining_length)) {
      // write failed, flush...
      buffer[buffer_offset] = '\0';
      ESP_LOGVV(TAG, "%s", buffer);
      buffer_offset = 0;
      written = sprintf(buffer, "  ");
      if (i + 1 < vec.size()) {
        written += sprintf(buffer + written, "%" PRId32 ", ", value);
      } else {
        written += sprintf(buffer + written, "%" PRId32, value);
      }
    }

    buffer_offset += written;
  }
  if (buffer_offset != 0) {
    ESP_LOGVV(TAG, "%s", buffer);
  }
#endif
  this->send_internal(send_times, send_wait);
}
}  // namespace remote_base
}  // namespace esphome

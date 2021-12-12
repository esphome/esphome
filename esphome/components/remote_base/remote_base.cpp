#include "remote_base.h"
#include "esphome/core/log.h"

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

void RemoteReceiverBinarySensorBase::dump_config() { LOG_BINARY_SENSOR("", "Remote Receiver Binary Sensor", this); }

void RemoteTransmitterBase::send_(uint32_t send_times, uint32_t send_wait) {
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  const std::vector<int32_t> &vec = this->temp_.get_data();
  char buffer[256];
  uint32_t buffer_offset = 0;
  buffer_offset += sprintf(buffer, "Sending times=%u wait=%ums: ", send_times, send_wait);

  for (size_t i = 0; i < vec.size(); i++) {
    const int32_t value = vec[i];
    const uint32_t remaining_length = sizeof(buffer) - buffer_offset;
    int written;

    if (i + 1 < vec.size()) {
      written = snprintf(buffer + buffer_offset, remaining_length, "%d, ", value);
    } else {
      written = snprintf(buffer + buffer_offset, remaining_length, "%d", value);
    }

    if (written < 0 || written >= int(remaining_length)) {
      // write failed, flush...
      buffer[buffer_offset] = '\0';
      ESP_LOGVV(TAG, "%s", buffer);
      buffer_offset = 0;
      written = sprintf(buffer, "  ");
      if (i + 1 < vec.size()) {
        written += sprintf(buffer + written, "%d, ", value);
      } else {
        written += sprintf(buffer + written, "%d", value);
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

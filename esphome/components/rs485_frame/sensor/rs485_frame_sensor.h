#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../rs485_frame.h"

namespace esphome::rs485_frame {

/// Diagnostic sensor that publishes a hub state value (frames received, CRC failures,
/// queue depth, etc.) on change only. User payload decoding is done via on_frame: +
/// globals: + template sensors; this platform is only for hub diagnostics.
class RS485FrameSensor : public sensor::Sensor, public Component {
 public:
  void set_parent(RS485FrameHub *parent) { this->parent_ = parent; }
  void set_decode(SensorDecode decode) { this->decode_ = decode; }
#ifdef USE_RS485_FRAME_RESPONSE_MONITOR
  // Selects a response_monitor: entry's occurrence counter instead of a plain SensorDecode —
  // mutually exclusive with set_decode() (the Python schema enforces exactly one is called).
  void set_response_monitor(uint8_t entry_index, ResponseMonitorStat stat) {
    this->is_response_monitor_ = true;
    this->monitor_entry_index_ = entry_index;
    this->monitor_stat_ = stat;
  }
#endif

  void loop() override {
    if (this->parent_ == nullptr)
      return;
    float value = 0.0f;
#ifdef USE_RS485_FRAME_RESPONSE_MONITOR
    if (this->is_response_monitor_) {
      value =
          static_cast<float>(this->parent_->get_response_monitor_stat(this->monitor_entry_index_, this->monitor_stat_));
    } else
#endif
    {
      switch (this->decode_) {
        case SENSOR_DECODE_FRAMES_RECEIVED:
          value = static_cast<float>(this->parent_->get_frames_received());
          break;
        case SENSOR_DECODE_CRC_FAILURES:
          value = static_cast<float>(this->parent_->get_crc_failures());
          break;
        case SENSOR_DECODE_COMMANDS_SENT:
          value = static_cast<float>(this->parent_->get_commands_sent());
          break;
        case SENSOR_DECODE_COMMAND_DROPS:
          value = static_cast<float>(this->parent_->get_command_drops());
          break;
        case SENSOR_DECODE_LAST_KEEPALIVE_MS:
          value = static_cast<float>(this->parent_->get_last_keepalive_ms());
          break;
        case SENSOR_DECODE_QUEUE_DEPTH:
          value = static_cast<float>(this->parent_->get_queue_depth());
          break;
      }
    }
    // Publish only on change: sensor::Sensor::publish_state does not deduplicate
    // internally, so we gate here to avoid per-loop API/log traffic when idle.
    if (!this->has_published_ || value != this->last_value_) {
      this->publish_state(value);
      this->last_value_ = value;
      this->has_published_ = true;
    }
  }
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  RS485FrameHub *parent_{nullptr};
  SensorDecode decode_{SENSOR_DECODE_FRAMES_RECEIVED};
#ifdef USE_RS485_FRAME_RESPONSE_MONITOR
  bool is_response_monitor_{false};
  uint8_t monitor_entry_index_{0};
  ResponseMonitorStat monitor_stat_{RESPONSE_MONITOR_STAT_SUCCESS};
#endif
  float last_value_{0.0f};
  bool has_published_{false};
};

}  // namespace esphome::rs485_frame

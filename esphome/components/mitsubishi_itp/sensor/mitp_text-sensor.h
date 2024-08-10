#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "../mitp_listener.h"

namespace esphome {
namespace mitsubishi_itp {

class MITPTextSensor : public MITPListener, public text_sensor::TextSensor {
 public:
  void publish(bool force = true) {
    // Only publish if force, or a change has occurred and we have a real value
    if (force || (mitp_text_sensor_state_.has_value() && mitp_text_sensor_state_.value() != state)) {
      publish_state(mitp_text_sensor_state_.value());
    }
  }

 protected:
  optional<std::string> mitp_text_sensor_state_;
};

class ActualFanSensor : public MITPTextSensor {
  void process_packet(const RunStateGetResponsePacket &packet) {
    mitp_text_sensor_state_ = ACTUAL_FAN_SPEED_NAMES[packet.get_actual_fan_speed()];
  }
};

class ErrorCodeSensor : public MITPTextSensor {
  void process_packet(const ErrorStateGetResponsePacket &packet) {
    // TODO: Include friendly text from JSON, somehow.
    if (!packet.error_present()) {
      mitp_text_sensor_state_ = std::string("No Error Reported");
    } else if (auto raw_code = packet.get_raw_short_code() != 0x00) {
      // Not that it matters, but good for validation I guess.
      if ((raw_code & 0x1F) > 0x15) {
        ESP_LOGW(LISTENER_TAG, "Error short code %x had invalid low bits. This is an IT protocol violation!", raw_code);
      }

      mitp_text_sensor_state_ = "Error " + packet.get_short_code();
    } else {
      mitp_text_sensor_state_ = "Error " + to_string(packet.get_error_code());
    }
  }
};

}  // namespace mitsubishi_itp
}  // namespace esphome

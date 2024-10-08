#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "../mitp_listener.h"

namespace esphome {
namespace mitsubishi_itp {

class MITPTextSensor : public MITPListener, public text_sensor::TextSensor {
 public:
  void publish() override {
    // Only publish if force, or a change has occurred and we have a real value
    if (mitp_text_sensor_state_.has_value() && mitp_text_sensor_state_.value() != state) {
      publish_state(mitp_text_sensor_state_.value());
    }
  }

 protected:
  optional<std::string> mitp_text_sensor_state_;
};

class ActualFanSensor : public MITPTextSensor {
  void process_packet(const RunStateGetResponsePacket &packet) override {
    mitp_text_sensor_state_ = ACTUAL_FAN_SPEED_NAMES[packet.get_actual_fan_speed()];
  }
};

class ErrorCodeSensor : public MITPTextSensor {
  void process_packet(const ErrorStateGetResponsePacket &packet) override;
};

class ThermostatBatterySensor : public MITPTextSensor {
  void process_packet(const ThermostatSensorStatusPacket &packet) override;
};

}  // namespace mitsubishi_itp
}  // namespace esphome

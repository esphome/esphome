#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../mitp_listener.h"

namespace esphome {
namespace mitsubishi_itp {

class MITPBinarySensor : public MITPListener, public binary_sensor::BinarySensor {
 public:
  void publish(bool force = true) {
    if (mitp_binary_sensor_state_.has_value())
      // Binary sensors automatically dedup publishes (I think) and so will only actually publish on change
      publish_state(mitp_binary_sensor_state_.value());
  }

 protected:
  optional<bool> mitp_binary_sensor_state_;
};

class DefrostSensor : public MITPBinarySensor {
  void process_packet(const RunStateGetResponsePacket &packet) { mitp_binary_sensor_state_ = packet.in_defrost(); }
};
class FilterStatusSensor : public MITPBinarySensor {
  void process_packet(const RunStateGetResponsePacket &packet) { mitp_binary_sensor_state_ = packet.service_filter(); }
};
class PreheatSensor : public MITPBinarySensor {
  void process_packet(const RunStateGetResponsePacket &packet) { mitp_binary_sensor_state_ = packet.in_preheat(); }
};
class StandbySensor : public MITPBinarySensor {
  void process_packet(const RunStateGetResponsePacket &packet) { mitp_binary_sensor_state_ = packet.in_standby(); }
};
class ISeeStatusSensor : public MITPBinarySensor {
  void process_packet(const SettingsGetResponsePacket &packet) {
    mitp_binary_sensor_state_ = packet.is_i_see_enabled();
  }
};

}  // namespace mitsubishi_itp
}  // namespace esphome

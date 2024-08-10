#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../mitp_listener.h"

namespace esphome {
namespace mitsubishi_itp {

class MITPSensor : public MITPListener, public sensor::Sensor {
 public:
  void publish() override {
    // Only publish if force, or a change has occurred and we have a real value
    if (!isnan(mitp_sensor_state_) && mitp_sensor_state_ != state) {
      publish_state(mitp_sensor_state_);
    }
  }

 protected:
  float mitp_sensor_state_ = NAN;
};

class CompressorFrequencySensor : public MITPSensor {
  void process_packet(const StatusGetResponsePacket &packet) { mitp_sensor_state_ = packet.get_compressor_frequency(); }
};

class OutdoorTemperatureSensor : public MITPSensor {
  void process_packet(const CurrentTempGetResponsePacket &packet) { mitp_sensor_state_ = packet.get_outdoor_temp(); }
};

class ThermostatHumiditySensor : public MITPSensor {
  void process_packet(const ThermostatSensorStatusPacket &packet) {
    mitp_sensor_state_ = packet.get_indoor_humidity_percent();
  }
};

class ThermostatTemperatureSensor : public MITPSensor {
  void process_packet(const RemoteTemperatureSetRequestPacket &packet) {
    mitp_sensor_state_ = packet.get_remote_temperature();
  }
};

}  // namespace mitsubishi_itp
}  // namespace esphome

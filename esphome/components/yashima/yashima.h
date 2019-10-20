#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace yashima {

class YashimaClimate : public climate::Climate, public Component {
 public:
  void setup() override;
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
  void set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

 protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Transmit via IR the state of this climate controller.
  void transmit_state_();

  bool supports_cool_{true};
  bool supports_heat_{true};

  remote_transmitter::RemoteTransmitterComponent *transmitter_;
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace yashima
}  // namespace esphome

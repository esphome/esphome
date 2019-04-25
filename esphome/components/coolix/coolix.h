#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace coolix {

class CoolixClimate : public climate::Climate, public Component {
 public:
  CoolixClimate(const std::string &name) : climate::Climate(name) {}

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

  void send_data_(remote_base::RemoteTransmitData * transmit_data, uint16_t onemark, uint32_t onespace,
                  uint16_t zeromark, uint32_t zerospace, uint64_t data, uint16_t nbits, bool msb_first = true);

  bool supports_cool_{true};
  bool supports_heat_{true};

  remote_transmitter::RemoteTransmitterComponent *transmitter_;
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace coolix
}  // namespace esphome

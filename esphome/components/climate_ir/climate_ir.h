#pragma once

#include <utility>

#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace climate_ir {

/* A base for climate which works by sending (and receiving) IR codes

    To send IR codes implement
      void ClimateIR::transmit_state_()

    Likewise to decode a IR into the AC state, implement
      bool RemoteReceiverListener::on_receive(remote_base::RemoteReceiveData data) and return true
*/
class ClimateIR : public climate::Climate, public Component, public remote_base::RemoteReceiverListener {
 public:
  ClimateIR(float minimum_temperature, float maximum_temperature, float temperature_step = 1.0f,
            bool supports_dry = false, bool supports_fan_only = false, std::set<climate::ClimateFanMode> fan_modes = {},
            std::set<climate::ClimateSwingMode> swing_modes = {}) {
    this->minimum_temperature_ = minimum_temperature;
    this->maximum_temperature_ = maximum_temperature;
    this->temperature_step_ = temperature_step;
    this->supports_dry_ = supports_dry;
    this->supports_fan_only_ = supports_fan_only;
    this->fan_modes_ = std::move(fan_modes);
    this->swing_modes_ = std::move(swing_modes);
  }

  void setup() override;
  void dump_config() override;
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
  void set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

 protected:
  float minimum_temperature_, maximum_temperature_, temperature_step_;

  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  /// Transmit via IR the state of this climate controller.
  virtual void transmit_state() = 0;

  // Dummy implement on_receive so implementation is optional for inheritors
  bool on_receive(remote_base::RemoteReceiveData data) override { return false; };

  bool supports_cool_{true};
  bool supports_heat_{true};
  bool supports_dry_{false};
  bool supports_fan_only_{false};
  std::set<climate::ClimateFanMode> fan_modes_ = {};
  std::set<climate::ClimateSwingMode> swing_modes_ = {};

  remote_transmitter::RemoteTransmitterComponent *transmitter_;
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace climate_ir
}  // namespace esphome

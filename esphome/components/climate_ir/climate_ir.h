#pragma once

#include <utility>

#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::climate_ir {

/* A base for climate which works by sending (and receiving) IR codes

    To send IR codes implement
      void ClimateIR::transmit_state_()

    Likewise to decode a IR into the AC state, implement
      bool RemoteReceiverListener::on_receive(remote_base::RemoteReceiveData data) and return true
*/
class ClimateIR : public Component,
                  public climate::Climate,
                  public remote_base::RemoteReceiverListener,
                  public remote_base::RemoteTransmittable {
 public:
  ClimateIR(float minimum_temperature, float maximum_temperature, float temperature_step = 1.0f,
            bool supports_dry = false, bool supports_fan_only = false,
            climate::ClimateFanModeMask fan_modes = climate::ClimateFanModeMask(),
            climate::ClimateSwingModeMask swing_modes = climate::ClimateSwingModeMask(),
            climate::ClimatePresetMask presets = climate::ClimatePresetMask()) {
    this->minimum_temperature_ = minimum_temperature;
    this->maximum_temperature_ = maximum_temperature;
    this->temperature_step_ = temperature_step;
    if (supports_dry)
      this->modes_.insert(climate::CLIMATE_MODE_DRY);
    if (supports_fan_only)
      this->modes_.insert(climate::CLIMATE_MODE_FAN_ONLY);
    this->fan_modes_ = fan_modes;
    this->swing_modes_ = swing_modes;
    this->presets_ = presets;
  }

  void setup() override;
  void dump_config() override;
  void set_supports_cool(bool supports_cool) { this->set_mode_supported_(climate::CLIMATE_MODE_COOL, supports_cool); }
  void set_supports_heat(bool supports_heat) { this->set_mode_supported_(climate::CLIMATE_MODE_HEAT, supports_heat); }
  void set_supports_heat_cool(bool supports_heat_cool) {
    this->set_mode_supported_(climate::CLIMATE_MODE_HEAT_COOL, supports_heat_cool);
  }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_humidity_sensor(sensor::Sensor *sensor) { this->humidity_sensor_ = sensor; }

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

  ESPHOME_ALWAYS_INLINE void set_mode_supported_(climate::ClimateMode mode, bool supported) {
    if (supported) {
      this->modes_.insert(mode);
    } else {
      this->modes_.erase(mode);
    }
  }

  // The HEAT_COOL default (supports_cool && supports_heat) is resolved during code generation.
  static constexpr climate::ClimateModeMask DEFAULT_MODES{climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL,
                                                          climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_HEAT_COOL};
  climate::ClimateModeMask modes_{DEFAULT_MODES};
  climate::ClimateFanModeMask fan_modes_{};
  climate::ClimateSwingModeMask swing_modes_{};
  climate::ClimatePresetMask presets_{};

  sensor::Sensor *sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
};

}  // namespace esphome::climate_ir

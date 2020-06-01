#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"

#ifdef USE_POWER_SUPPLY
#include "esphome/components/power_supply/power_supply.h"
#endif

namespace esphome {
namespace easyscale {

class EasyScale : public light::LightOutput, public Component {
 public:
  virtual ~EasyScale() = default;

#ifdef USE_POWER_SUPPLY
  /** Use this to connect up a power supply to this output.
   *
   * Whenever this output is enabled, the power supply will automatically be turned on.
   *
   * @param power_supply The PowerSupplyComponent, set this to nullptr to disable the power supply.
   */
  void set_power_supply(power_supply::PowerSupply *power_supply) { this->power_.set_parent(power_supply); }
#endif

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }

  void setup() override;

  void setup_state(light::LightState *state) override;
  void update_state(light::LightState *state) override{};
  void write_state(light::LightState *state) override;

 protected:
  InternalGPIOPin *pin_;
  uint8_t device_address_ = 0x72;

#ifdef USE_POWER_SUPPLY
  power_supply::PowerSupplyRequester power_{};
#endif

 private:
  inline void send_zero_();
  inline void send_one_();
};

}  // namespace easyscale
}  // namespace esphome
#endif
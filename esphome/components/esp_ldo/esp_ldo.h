#pragma once
#ifdef USE_ESP32_VARIANT_ESP32P4
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esp_ldo_regulator.h"

namespace esphome {
namespace esp_ldo {

class EspLdo : public Component {
 public:
  EspLdo(int channel) : channel_(channel) {}

  void setup() override;
  void dump_config() override;

  void set_adjustable(bool adjustable) { this->adjustable_ = adjustable; }
  void set_voltage(float voltage) { this->voltage_ = voltage; }
  void adjust_voltage(float voltage);
  float get_setup_priority() const override {
    return setup_priority::BUS;  // LDO setup should be done early
  }

 protected:
  int channel_;
  float voltage_{2.7};
  bool adjustable_{false};
  esp_ldo_channel_handle_t handle_{};
};

template<typename... Ts> class AdjustAction : public Action<Ts...> {
 public:
  explicit AdjustAction(EspLdo *ldo) : ldo_(ldo) {}

  TEMPLATABLE_VALUE(float, voltage)

  void play(const Ts &...x) override { this->ldo_->adjust_voltage(this->voltage_.value(x...)); }

 protected:
  EspLdo *ldo_;
};

}  // namespace esp_ldo
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32P4

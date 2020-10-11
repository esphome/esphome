#pragma once

#include "esphome/core/component.h"
#include "esphome/components/midea_dongle/midea_dongle.h"
#include "esphome/components/climate/climate.h"
#include "midea_frame.h"

namespace esphome {
namespace midea_ac {

class MideaClimate : public climate::Climate, public PollingComponent {
public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void update() override;
  void set_midea_dongle_parent(midea_dongle::MideaDongle *parent) { this->parent_ = parent; }
  void set_beeper_feedback(bool state) { this->beeper_feedback_ = state; }
protected:
  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override;
  /// Return the traits of this controller.
  climate::ClimateTraits traits() override;

  const QueryFrame query_frame_;
  CommandFrame cmd_frame_;
  uint8_t need_request_{0};
  bool beeper_feedback_{false};

  enum MideaMessages : uint8_t
  {
    MSG_CONTROL = 0x01,
    MSG_NETWORK = 0x02
  };

  midea_dongle::MideaDongle *parent_;
};

}  // namespace midea_ac
}  // namespace esphome

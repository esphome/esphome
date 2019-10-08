#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya/tuya.h"
#include "esphome/components/light/light_output.h"

namespace esphome {
namespace tuya {

class TuyaLight : public Component, public light::LightOutput, public TuyaListener {
 public:
  void setup() override;
  void dump_config() override;
  void set_dimmer(int dpid);
  void set_switch(int dpid);
  void set_tuya_parent(Tuya *parent);
  void set_min_value(uint32_t value);
  void set_max_value(uint32_t value);
  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override;
  void write_state(light::LightState *state) override;
  void dp_update(int dpid, uint32_t value) override;

 protected:
  void update_dimmer_(uint32_t value);
  void update_switch_(uint32_t value);

  Tuya *parent_;
  int dimmer_id_ = -1;
  int switch_id_ = -1;
  uint32_t dimmer_value_ = 0;
  uint32_t switch_value_ = 0;
  uint32_t min_value_ = 0;
  uint32_t max_value_ = 255;
  light::LightState *state_{nullptr};
};

}  // namespace tuya
}  // namespace esphome

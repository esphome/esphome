#pragma once
#ifdef USE_ESP32

#include "esphome/components/light/light_output.h"
#include "esphome/components/switch/switch.h"
#include "fendt_caravan_hub_base.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "device_decoders.h"
#include "fendt_caravan.h"
#include <map>
#include <string>

namespace esphome::fendt_caravan {

class LightingDeviceHub : public FendtCaravanHubBase, public Parented<FendtCaravan> {
 public:
  void dump_config() override;

  void set_binary_light_state(const std::string &key_name, light::LightState *state);
  void set_float_light_state(const std::string &key_name, light::LightState *state);
  void write_state(switch_::Switch *sw, bool state) override {};
  bool decode(const char *key, const char *value) override;

 private:
  std::map<std::string, light::LightState *> binary_states_{};
  std::map<std::string, light::LightState *> float_states_{};
};

}  // namespace esphome::fendt_caravan
#endif

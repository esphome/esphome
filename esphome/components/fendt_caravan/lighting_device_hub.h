#pragma once
#ifdef USE_ESP32

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "fendt_caravan.h"
#include <map>
#include <string>

namespace esphome::fendt_caravan {

class LightingDeviceHub : public FendtCaravanHubBase, public Parented<FendtCaravan> {
 public:
  void dump_config() override;

  void add_binary_lightoutput(const std::string &key_name, light::LightOutput *light_output);
  void add_float_lightoutput(const std::string &key_name, light::LightOutput *light_output);

 private:
  std::map<std::string, light::LightOutput *> binary_outputs_{};
  std::map<std::string, light::LightOutput *> float_outputs_{};
};

}  // namespace esphome::fendt_caravan
#endif

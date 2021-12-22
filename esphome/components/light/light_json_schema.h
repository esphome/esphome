#pragma once

#include "esphome/core/defines.h"

#ifdef USE_JSON

#include "esphome/components/json/json_util.h"
#include "light_call.h"
#include "light_state.h"

namespace esphome {
namespace light {

class LightJSONSchema {
 public:
  /// Dump the state of a light as JSON.
  static void dump_json(LightState &state, JsonObject &root);
  /// Parse the JSON state of a light to a LightCall.
  static void parse_json(LightState &state, LightCall &call, JsonObject &root);

 protected:
  static void parse_color_json(LightState &state, LightCall &call, JsonObject &root);
};

}  // namespace light
}  // namespace esphome

#endif

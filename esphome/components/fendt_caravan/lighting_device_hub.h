#pragma once
#ifdef USE_ESP32

#include "esphome/components/light/light_output.h"
#include "esphome/components/switch/switch.h"
#include "fendt_caravan_hub_base.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "device_decoders.h"
#include "device_commands.h"
#include "fendt_caravan.h"

namespace esphome::fendt_caravan {

struct LightEntry {
  StringRef key;             // Non-owning, points to flash literal
  light::LightState *state;  // Pointer to light state
  bool is_on;
};

class LightingDeviceHub : public FendtCaravanHubBase, public Parented<FendtCaravan> {
 public:
  void dump_config() override;
  void set_binary_light_state(const char *key_name, light::LightState *state);
  void set_float_light_state(const char *key_name, light::LightState *state);
  bool decode(const char *key, const char *value) override;
  void write_state(const char *key_name, light::LightState *state);

 protected:
#ifdef LIGHTING_DEVICE_BINARY_LIGHT_COUNT
  StaticVector<LightEntry, LIGHTING_DEVICE_BINARY_LIGHT_COUNT> binary_states_;
#endif
#ifdef LIGHTING_DEVICE_FLOAT_LIGHT_COUNT
  StaticVector<LightEntry, LIGHTING_DEVICE_FLOAT_LIGHT_COUNT> float_states_;
#endif
};

}  // namespace esphome::fendt_caravan
#endif

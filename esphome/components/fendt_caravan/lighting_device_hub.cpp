#include "lighting_device_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.lights");

void LightingDeviceHub::dump_config() { ESP_LOGCONFIG(TAG, "-Fendt Lighting Device-"); }

void LightingDeviceHub::set_binary_light_state(const char *key_name, light::LightState *state) {
#ifdef LIGHTING_DEVICE_BINARY_LIGHT_COUNT
  this->binary_states_.push_back({StringRef(key_name), state});
#endif
}

void LightingDeviceHub::set_float_light_state(const char *key_name, light::LightState *state) {
#ifdef LIGHTING_DEVICE_FLOAT_LIGHT_COUNT
  this->float_states_.push_back({StringRef(key_name), state});
#endif
}

bool LightingDeviceHub::decode(const char *key, const char *value) {
#ifdef LIGHTING_DEVICE_BINARY_LIGHT_COUNT
  for (auto &entry : this->binary_states_) {
    if (entry.key == key) {
      if (entry.state) {
        int decoded_value = DeviceDecoders::decode_int(value);
        entry.state->remote_values.set_state(decoded_value != 0);
        entry.state->current_values.set_state(decoded_value != 0);
        entry.state->publish_state();
        return true;
      }
    }
  }
#endif  // LIGHTING_DEVICE_BINARY_LIGHT_COUNT
#ifdef LIGHTING_DEVICE_FLOAT_LIGHT_COUNT
  for (auto &entry : this->float_states_) {
    if (entry.key == key) {
      if (entry.state) {
        int decoded_value = DeviceDecoders::decode_int(value);
        entry.state->remote_values.set_state(decoded_value != 0);
        entry.state->current_values.set_state(decoded_value != 0);
        entry.state->remote_values.set_brightness((float) decoded_value / 15.0f);
        entry.state->publish_state();
        return true;
      }
    }
  }
#endif  // LIGHTING_DEVICE_FLOAT_LIGHT_COUNT
  return false;
}

}  // namespace esphome::fendt_caravan
#endif

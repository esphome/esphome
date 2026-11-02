#include "lighting_device_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.lights");

void LightingDeviceHub::dump_config() { ESP_LOGCONFIG(TAG, "-Fendt Lighting Device-"); }

void LightingDeviceHub::set_binary_light_state(const std::string &key_name, light::LightState *state) {
  this->binary_states_[key_name] = state;
}

void LightingDeviceHub::set_float_light_state(const std::string &key_name, light::LightState *state) {
  this->float_states_[key_name] = state;
}

bool LightingDeviceHub::decode(const char *key, const char *value) {
  ESP_LOGD(TAG, "Decoding key: %s, value: %s", key, value);
  auto binary_it = this->binary_states_.find(key);
  if (binary_it != this->binary_states_.end()) {
    auto *state = binary_it->second;
    if (state) {
      int decoded_value = DeviceDecoders::decode_int(value);
      state->remote_values.set_state(decoded_value != 0);
      state->publish_state();
      return true;
    }
  }
  auto float_it = this->float_states_.find(key);
  if (float_it != this->float_states_.end()) {
    auto *state = float_it->second;
    if (state) {
      int decoded_value = DeviceDecoders::decode_int(value);
      state->remote_values.set_state(decoded_value != 0);
      state->remote_values.set_brightness((float) decoded_value / 15.0f);
      state->publish_state();
      return true;
    }
  }
  return false;
}

}  // namespace esphome::fendt_caravan
#endif

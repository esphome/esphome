#include "lighting_device_hub.h"

#ifdef USE_ESP32
namespace esphome::fendt_caravan {
ESPHOME_LOG_TAG(TAG, "FC.lights");

void LightingDeviceHub::dump_config() { ESP_LOGCONFIG(TAG, "-Fendt Lighting Device-"); }

void LightingDeviceHub::add_binary_lightoutput(const std::string &key_name, light::LightOutput *light_output) {
  this->binary_outputs_[key_name] = light_output;
}

void LightingDeviceHub::add_float_lightoutput(const std::string &key_name, light::LightOutput *light_output) {
  this->float_outputs_[key_name] = light_output;
}

}  // namespace esphome::fendt_caravan
#endif

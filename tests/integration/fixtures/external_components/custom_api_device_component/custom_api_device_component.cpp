#include "custom_api_device_component.h"
#include "esphome/core/log.h"

#ifdef USE_API
namespace esphome {
namespace custom_api_device_component {

static const char *const TAG = "custom_api";

void CustomAPIDeviceComponent::setup() {
  // Register services using CustomAPIDevice
  register_service(&CustomAPIDeviceComponent::on_test_service, "custom_test_service");

  register_service(&CustomAPIDeviceComponent::on_service_with_args, "custom_service_with_args",
                   {"arg_string", "arg_int", "arg_bool", "arg_float"});

  // Test array types
  register_service(&CustomAPIDeviceComponent::on_service_with_arrays, "custom_service_with_arrays",
                   {"bool_array", "int_array", "float_array", "string_array"});

  // Test Home Assistant state subscription using std::string API (custom_api_device.h)
  // This tests the backward compatibility of the std::string overloads
  subscribe_homeassistant_state(&CustomAPIDeviceComponent::on_ha_state_changed, std::string("sensor.custom_test"));
}

void CustomAPIDeviceComponent::on_test_service() { ESP_LOGI(TAG, "Custom test service called!"); }

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void CustomAPIDeviceComponent::on_service_with_args(std::string arg_string, int32_t arg_int, bool arg_bool,
                                                    float arg_float) {
  ESP_LOGI(TAG, "Custom service called with: %s, %d, %d, %.2f", arg_string.c_str(), arg_int, arg_bool, arg_float);
}

void CustomAPIDeviceComponent::on_service_with_arrays(std::vector<bool> bool_array, std::vector<int32_t> int_array,
                                                      std::vector<float> float_array,
                                                      std::vector<std::string> string_array) {
  ESP_LOGI(TAG, "Array service called with %zu bools, %zu ints, %zu floats, %zu strings", bool_array.size(),
           int_array.size(), float_array.size(), string_array.size());

  // Log first element of each array if not empty
  if (!bool_array.empty()) {
    ESP_LOGI(TAG, "First bool: %s", bool_array[0] ? "true" : "false");
  }
  if (!int_array.empty()) {
    ESP_LOGI(TAG, "First int: %d", int_array[0]);
  }
  if (!float_array.empty()) {
    ESP_LOGI(TAG, "First float: %.2f", float_array[0]);
  }
  if (!string_array.empty()) {
    ESP_LOGI(TAG, "First string: %s", string_array[0].c_str());
  }
}

void CustomAPIDeviceComponent::on_ha_state_changed(std::string entity_id, std::string state) {
  ESP_LOGI(TAG, "Home Assistant state changed for %s: %s", entity_id.c_str(), state.c_str());
  ESP_LOGI(TAG, "This subscription uses std::string API for backward compatibility");
}

}  // namespace custom_api_device_component
}  // namespace esphome
#endif  // USE_API

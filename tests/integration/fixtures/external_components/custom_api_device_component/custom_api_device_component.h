#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/api/custom_api_device.h"

#ifdef USE_API
namespace esphome {
namespace custom_api_device_component {

using namespace api;

class CustomAPIDeviceComponent : public Component, public CustomAPIDevice {
 public:
  void setup() override;

  void on_test_service();

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  void on_service_with_args(std::string arg_string, int32_t arg_int, bool arg_bool, float arg_float);

  void on_service_with_arrays(std::vector<bool> bool_array, std::vector<int32_t> int_array,
                              std::vector<float> float_array, std::vector<std::string> string_array);

  // Test Home Assistant state subscription with std::string API
  void on_ha_state_changed(std::string entity_id, std::string state);
};

}  // namespace custom_api_device_component
}  // namespace esphome
#endif  // USE_API

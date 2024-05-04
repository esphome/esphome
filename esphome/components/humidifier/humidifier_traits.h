#pragma once

#include "esphome/core/helpers.h"
#include "humidifier_mode.h"
#include <set>

namespace esphome {
namespace humidifier {

/** This class contains all static data for humidifier devices.
 *
 * All humidifier devices must support these features:
 *  - OFF mode
 *  - Target Humidity
 *
 * All other properties and modes are optional and the integration must mark
 * each of them as supported by setting the appropriate flag here.
 *
 *  - supports current humidity - if the humidifier device supports reporting a current humidity
 *  - supports two point target humidity - if the humidifier device's target humidity should be
 *     split in target_humidity_low and target_humidity_high instead of just the single target_humidity
 *  - supports modes:
 *    - auto mode (automatic control)
 *    - dehumidify mode (removes humidity from air)
 *    - humidify mode (increases current humidity)
 *  - supports action - if the humidifier device supports reporting the active
 *    current action of the device with the action property.
 *
 * This class also contains static data for the humidifier device display:
 *  - visual min/max humidity - tells the frontend what range of humiditys the humidifier device
 *     should display (gauge min/max values)
 *  - humidity step - the step with which to increase/decrease target humidity.
 *     This also affects with how many decimal places the humidity is shown
 */
class HumidifierTraits {
 public:
  bool get_supports_current_humidity() const { return supports_current_humidity_; }
  void set_supports_current_humidity(bool supports_current_humidity) {
    supports_current_humidity_ = supports_current_humidity;
  }
  bool get_supports_two_point_target_humidity() const { return supports_two_point_target_humidity_; }
  void set_supports_two_point_target_humidity(bool supports_two_point_target_humidity) {
    supports_two_point_target_humidity_ = supports_two_point_target_humidity;
  }
  void set_supported_modes(std::set<HumidifierMode> modes) { supported_modes_ = std::move(modes); }
  void add_supported_mode(HumidifierMode mode) { supported_modes_.insert(mode); }
  bool supports_mode(HumidifierMode mode) const { return supported_modes_.count(mode); }
  std::set<HumidifierMode> get_supported_modes() const { return supported_modes_; }

  void set_supports_action(bool supports_action) { supports_action_ = supports_action; }
  bool get_supports_action() const { return supports_action_; }

  void set_supported_presets(std::set<HumidifierPreset> presets) { supported_presets_ = std::move(presets); }
  void add_supported_preset(HumidifierPreset preset) { supported_presets_.insert(preset); }
  bool supports_preset(HumidifierPreset preset) const { return supported_presets_.count(preset); }
  bool get_supports_presets() const { return !supported_presets_.empty(); }
  const std::set<humidifier::HumidifierPreset> &get_supported_presets() const { return supported_presets_; }

  float get_visual_min_humidity() const { return visual_min_humidity_; }
  void set_visual_min_humidity(float visual_min_humidity) { visual_min_humidity_ = visual_min_humidity; }
  float get_visual_max_humidity() const { return visual_max_humidity_; }
  void set_visual_max_humidity(float visual_max_humidity) { visual_max_humidity_ = visual_max_humidity; }
  float get_visual_humidity_step() const { return visual_humidity_step_; }
  int8_t get_humidity_accuracy_decimals() const;
  void set_visual_humidity_step(float humidity_step) { visual_humidity_step_ = humidity_step; }

 protected:
  void set_mode_support_(humidifier::HumidifierMode mode, bool supported) {
    if (supported) {
      supported_modes_.insert(mode);
    } else {
      supported_modes_.erase(mode);
    }
  }

  bool supports_current_humidity_{false};
  bool supports_two_point_target_humidity_{false};
  std::set<humidifier::HumidifierMode> supported_modes_ = {humidifier::HUMIDIFIER_MODE_OFF};
  bool supports_action_{false};
  std::set<humidifier::HumidifierPreset> supported_presets_;

  float visual_min_humidity_{20};
  float visual_max_humidity_{80};
  float visual_humidity_step_{0.1};
};

}  // namespace humidifier
}  // namespace esphome

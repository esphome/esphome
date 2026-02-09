

#pragma once
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include <functional>
#include "esphome/core/application.h"

namespace esphome {
namespace filter_lifetime {

class FilterLifetime : public sensor::Sensor, public PollingComponent {
 public:
  void reset_filter();
  void dump_config() override;
  void setup() override;
  void set_max_lifetime(int max_lifetime);
  void set_is_on(std::function<bool()> is_on);
  void set_current_speed(std::function<float()> current_speed);
  void update() override;

 protected:
  int max_lifetime_{0};  // months
  std::function<bool()> is_on_;
  std::function<float()> current_speed_;
  uint32_t last_update_{0};
  float runtime_minutes_{0.0f};
  ESPPreferenceObject pref_;
};

}  // namespace filter_lifetime
}  // namespace esphome

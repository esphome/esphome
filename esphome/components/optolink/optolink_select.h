#pragma once

#ifdef USE_ARDUINO

#include <map>
#include "esphome/components/select/select.h"
#include "optolink.h"
#include "optolink_sensor_base.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkSelect : public OptolinkSensorBase, public esphome::select::Select, public esphome::PollingComponent {
 public:
  OptolinkSelect(Optolink *optolink) : OptolinkSensorBase(optolink, true) {}

  void set_map(std::map<std::string, std::string> *mapping) {
    mapping_ = mapping;
    std::vector<std::string> values;
    for (auto &it : *mapping) {
      values.push_back(it.second);
    }
    traits.set_options(values);
  };

 protected:
  void setup() override { setup_datapoint_(); }
  void update() override { optolink_->read_value(datapoint_); }

  const StringRef &get_sensor_name() override { return get_name(); }
  void value_changed(float state) override;

  void control(const std::string &value) override;

 private:
  std::map<std::string, std::string> *mapping_ = nullptr;
};

}  // namespace optolink
}  // namespace esphome

#endif

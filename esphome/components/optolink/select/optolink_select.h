#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/select/select.h"
#include "../optolink.h"
#include "../datapoint_component.h"
#include <map>

namespace esphome {
namespace optolink {

class OptolinkSelect : public DatapointComponent, public esphome::select::Select, public esphome::PollingComponent {
 public:
  OptolinkSelect(Optolink *optolink) : DatapointComponent(optolink, true) {}

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
  void update() override { datapoint_read_request_(); }
  void control(const std::string &value) override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(const std::string &value) override;
  void datapoint_value_changed(uint8_t value) override;
  void datapoint_value_changed(uint16_t value) override;
  void datapoint_value_changed(uint32_t value) override;
  void datapoint_value_changed(float value) override;

 private:
  std::map<std::string, std::string> *mapping_ = nullptr;
};

}  // namespace optolink
}  // namespace esphome

#endif

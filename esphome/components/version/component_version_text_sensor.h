#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::version {

/// Reports a component's COMPONENT_VERSION instead of ESPHome's own version.
class ComponentVersionTextSensor final : public text_sensor::TextSensor, public Component {
 public:
  explicit ComponentVersionTextSensor(const char *component_name) : component_name_(component_name) {}
  /// Not called when the component declares no version, leaving the state unknown.
  void set_version(const char *version) { this->version_ = version; }
  void setup() override;
  void dump_config() override;

 protected:
  const char *component_name_;
  const char *version_{nullptr};
};

}  // namespace esphome::version

#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::version {

/// Reports a component's COMPONENT_VERSION instead of ESPHome's own version.
///
/// Both strings are flash-resident on esp8266, like the ESPHome-version path.
class ComponentVersionTextSensor final : public text_sensor::TextSensor, public Component {
 public:
  explicit ComponentVersionTextSensor(const LogString *component_name) : component_name_(component_name) {}
  /// Not called when the component declares no version, leaving the state unknown.
  void set_version(ProgmemStr version) { this->version_ = version; }
  void setup() override;
  void dump_config() override;

 protected:
  std::string version_string_() const;

  const LogString *component_name_;
  ProgmemStr version_{nullptr};
};

}  // namespace esphome::version

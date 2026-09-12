#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::version {

class VersionTextSensor final : public text_sensor::TextSensor, public Component {
 public:
  void set_hide_hash(bool hide_hash) { this->hide_hash_ = hide_hash; }
  /// Report this component's version instead of ESPHome's.
  ///
  /// Setting a name switches the sensor to component mode. If the component
  /// declares no version, set_version() is never called and the state is left
  /// unpublished, so it reads as unknown.
  void set_component_name(const char *name) { this->component_name_ = name; }
  void set_version(const char *version) { this->version_ = version; }
  void set_hide_timestamp(bool hide_timestamp) { this->hide_timestamp_ = hide_timestamp; }
  void setup() override;
  void dump_config() override;

 protected:
  bool hide_hash_{false};
  bool hide_timestamp_{false};
  const char *component_name_{nullptr};
  const char *version_{nullptr};
};

}  // namespace esphome::version

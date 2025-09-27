#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <string>

#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
#include "esphome/components/text_sensor/text_sensor.h"
#include "wrench.h"
#endif

namespace esphome {
namespace template_ {

 class TemplateBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void set_template(std::function<optional<bool>()> &&f) { this->f_ = f; }

#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
  ~TemplateBinarySensor();
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
  void set_dynamic(bool dynamic) { this->dynamic_ = dynamic; }
  void set_lambda_source(text_sensor::TextSensor *source);
#endif

 protected:
  std::function<optional<bool>()> f_{nullptr};

#ifdef USE_TEMPLATE_BINARY_SENSOR_DYNAMIC_LAMBDA
  bool ensure_runtime_();
  void destroy_runtime_();
  void execute_wrench_(const std::string &source);
  void handle_source_update_(const std::string &source);

  bool dynamic_{false};
  text_sensor::TextSensor *lambda_source_{nullptr};
  WRState *wrench_state_{nullptr};
  std::string last_source_;
#endif
};

}  // namespace template_
}  // namespace esphome

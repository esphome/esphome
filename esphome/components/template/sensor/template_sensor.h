#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <string>

#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
#include "esphome/components/text_sensor/text_sensor.h"
#include "wrench.h"
#endif

namespace esphome {
namespace template_ {

class TemplateSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_template(std::function<optional<float>()> &&f);

#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
  ~TemplateSensor();
#endif

  void update() override;

  void dump_config() override;

  float get_setup_priority() const override;

#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
  void set_dynamic(bool dynamic) { this->dynamic_ = dynamic; }
  void set_lambda_source(text_sensor::TextSensor *source);
#endif

 protected:
  optional<std::function<optional<float>()>> f_;

#ifdef USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA
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

#pragma once

#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/template_lambda.h"

namespace esphome::template_ {

class TemplateRealTimeClock : public time::RealTimeClock {
 public:
  template<typename F> void set_template(F &&f) { this->f_.set(std::forward<F>(f)); }

  /// The time is computed on demand and never written to the system clock, so there is nothing to poll.
  void update() override {}

  /// Returns 0 (an invalid time) if the lambda gives no value.
  time_t timestamp_now() override;

 protected:
  TemplateLambda<int64_t> f_;
};

}  // namespace esphome::template_

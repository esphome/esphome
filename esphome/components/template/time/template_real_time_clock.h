#pragma once

#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/template_lambda.h"

namespace esphome::template_ {

class TemplateRealTimeClock : public time::RealTimeClock {
 public:
  template<typename F> void set_template(F &&f) { this->f_.set(std::forward<F>(f)); }
  void set_sync_system_time(bool sync_system_time) { this->sync_system_time_ = sync_system_time; }

  /// If sync_system_time_ is set, periodically push this time to the system clock so that other
  /// code reading time via ::time(nullptr) (rather than this instance) sees it too.
  void update() override;

  time_t timestamp_now() override;

 protected:
  TemplateLambda<uint64_t> f_;
  bool sync_system_time_{false};
};

}  // namespace esphome::template_

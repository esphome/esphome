#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sun/sun.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace sun {

class SunTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_parent(Sun *parent) { parent_ = parent; }
  void set_clock(time::RealTimeClock *clock) { clock_ = clock; }
  void set_elevation(double elevation) { elevation_ = elevation; }
  void set_sunrise(bool sunrise) { sunrise_ = sunrise; }
  void set_format(const std::string &format) { format_ = format; }

  void update() override {
    optional<time::ESPTime> res;
    this->parent_->set_time(clock_->utcnow());
    if (this->sunrise_)
      res = this->parent_->sunrise(this->elevation_);
    else
      res = this->parent_->sunset(this->elevation_);
    if (!res) {
      this->publish_state("");
      return;
    }

    this->publish_state(res->strftime(this->format_));
  }

  void dump_config() override;

 protected:
  std::string format_{};
  Sun *parent_;
  time::RealTimeClock *clock_;
  double elevation_;
  bool sunrise_;
};

}  // namespace sun
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"

#include "esphome/components/sun/sun.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace sun {

class SunTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_parent(Sun *parent) { parent_ = parent; }
  void set_elevation(double elevation) { elevation_ = elevation; }
  void set_sunrise(bool sunrise) { sunrise_ = sunrise; }
  void set_format(const std::string &format) { format_ = format; }

  void update() override {
    optional<ESPTime> res;
    if (this->sunrise_) {
      res = this->parent_->sunrise(this->elevation_);
    } else {
      res = this->parent_->sunset(this->elevation_);
    }
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
  double elevation_;
  bool sunrise_;
};

}  // namespace sun
}  // namespace esphome

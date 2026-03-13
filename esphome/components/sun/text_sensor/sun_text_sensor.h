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
  void set_format(const char *format) { this->format_ = format; }
  /// Prevent accidental use of std::string which would dangle
  void set_format(const std::string &format) = delete;

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

    char buf[ESPTime::STRFTIME_BUFFER_SIZE];
    size_t len = res->strftime_to(buf, this->format_);
    this->publish_state(buf, len);
  }

  void dump_config() override;

 protected:
  const char *format_{nullptr};
  Sun *parent_;
  double elevation_;
  bool sunrise_;
};

}  // namespace sun
}  // namespace esphome

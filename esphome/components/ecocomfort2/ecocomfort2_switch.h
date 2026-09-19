#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome {
namespace ecocomfort2 {

class Ecocomfort2AdvancedSwitch : public switch_::Switch, public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_advanced_type(const char *type) { this->advanced_type_ = type; }

  // Ecocomfort2Client callbacks
  void on_status() override {}
  void on_config() override;
  void on_connect(bool) override {}
  const char *describe() const override { return "Ecocomfort2 Advanced Switch"; }

 protected:
  void write_state(bool state) override;
  const char *advanced_type_{nullptr};
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif

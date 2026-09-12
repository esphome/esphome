#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome {
namespace ecocomfort2 {

class Ecocomfort2SeasonSelect : public select::Select, public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Ecocomfort2Client callbacks
  void on_status() override {}
  void on_config() override;
  void on_connect(bool) override {}
  const char *describe() const override { return "Ecocomfort2 Season Select"; }

 protected:
  void control(size_t index) override;
};

class Ecocomfort2FreeCoolingSelect : public select::Select, public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Ecocomfort2Client callbacks
  void on_status() override {}
  void on_config() override;
  void on_connect(bool) override {}
  const char *describe() const override { return "Ecocomfort2 Free Cooling Select"; }

 protected:
  void control(size_t index) override;
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif

#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome {
namespace ecocomfort2 {

class Ecocomfort2ThresholdNumber : public number::Number, public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_threshold_type(const char *type) { this->threshold_type_ = type; }

  // Ecocomfort2Client callbacks
  void on_status() override {}
  void on_config() override;
  void on_connect(bool) override {}
  const char *describe() const override { return "Ecocomfort2 Threshold Number"; }

 protected:
  void control(float value) override;
  const char *threshold_type_{nullptr};
};

class Ecocomfort2OffsetNumber : public number::Number, public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_offset_type(const char *type) { this->offset_type_ = type; }

  // Ecocomfort2Client callbacks
  void on_status() override {}
  void on_config() override;
  void on_connect(bool) override {}
  const char *describe() const override { return "Ecocomfort2 Offset Number"; }

 protected:
  void control(float value) override;
  const char *offset_type_{nullptr};
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif

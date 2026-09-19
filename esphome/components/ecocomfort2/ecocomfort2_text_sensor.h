#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome {
namespace ecocomfort2 {

class Ecocomfort2TextSensor : public Ecocomfort2Client, public Component {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_firmware_sensor(text_sensor::TextSensor *s) { this->firmware_sensor_ = s; }

  // Ecocomfort2Client callbacks
  void on_status() override;
  void on_config() override {}
  void on_connect(bool) override {}
  const char *describe() const override { return "Ecocomfort2 Text Sensor"; }

 protected:
  text_sensor::TextSensor *firmware_sensor_{nullptr};
  char last_firmware_[Ecocomfort2Hub::FIRMWARE_VERSION_SIZE]{};
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif

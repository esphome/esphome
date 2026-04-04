#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome {
namespace ecocomfort2 {

class Ecocomfort2Fan : public fan::Fan, public Ecocomfort2Client, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  fan::FanTraits get_traits() override;

  // Ecocomfort2Client callbacks
  void on_status() override;
  void on_config() override {}
  void on_connect(bool connected) override;
  const char *describe() const override { return "Ecocomfort2 Fan"; }

 protected:
  void control(const fan::FanCall &call) override;

 private:
  /** Map preset name to OPER_* byte value. */
  uint8_t preset_name_to_mode_(const char *preset) const;
  /** Map OPER_* byte value to preset name. Returns nullptr if none. */
  const char *mode_to_preset_name_(uint8_t mode, bool auto_active) const;
  /** Map fan speed (1-4) to SPEED_* value. */
  uint8_t fan_speed_to_device_(int speed) const;
  /** Map SPEED_* value to fan speed (1-4). */
  int device_speed_to_fan_(uint8_t speed) const;
};

}  // namespace ecocomfort2
}  // namespace esphome

#endif

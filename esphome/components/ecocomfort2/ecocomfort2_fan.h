#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include "ecocomfort2_child.h"
#include "ecocomfort2_hub.h"

namespace esphome::ecocomfort2 {

class Ecocomfort2Fan : public fan::Fan, public Ecocomfort2Client, public Component {
 public:
  void setup() override;
  void dump_config() override;
  fan::FanTraits get_traits() override;
  void on_status() override;

 protected:
  void control(const fan::FanCall &call) override;
};

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32

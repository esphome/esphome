#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"

namespace esphome {
namespace psram {

class PsramComponent : public Component {
  void dump_config() override;
};

}  // namespace psram
}  // namespace esphome

#endif

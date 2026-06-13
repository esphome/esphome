#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"

namespace esphome::psram {

class PsramComponent : public Component {
  void dump_config() override;
};

}  // namespace esphome::psram

#endif

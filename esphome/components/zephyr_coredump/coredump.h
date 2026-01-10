#pragma once
#ifdef USE_ZEPHYR
#include "esphome/core/component.h"

namespace esphome::zephyr_coredump {

class Coredump : public Component {
 public:
  void dump_config() override;
};

}  // namespace esphome::zephyr_coredump
#endif

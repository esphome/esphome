#pragma once

#include "esphome/core/defines.h"

#if defined(USE_MIDEA_PURIFIER) && \
    ((defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF))
#include <cstdint>
#include "esphome/components/switch/switch.h"

namespace esphome::midea::ac {

class AirConditioner;

class PurifierSwitch final : public switch_::Switch {
 public:
  PurifierSwitch(AirConditioner *parent, uint32_t status_max_age) : parent_(parent), status_max_age_(status_max_age) {}
  void dump_config();

 protected:
  void write_state(bool state) override;
  AirConditioner *parent_;
  uint32_t status_max_age_;
};

}  // namespace esphome::midea::ac
#endif

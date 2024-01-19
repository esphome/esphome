#pragma once
#ifdef USE_NRF52

#include "esphome/core/optional.h"
#include "Arduino.h"

namespace esphome {
namespace deep_sleep {

class Nrf52DeepSleepBackend {
 public:
  void begin_sleep(const optional<uint64_t>& sleep_duration);
  void dump_config();
 protected:
  optional<uint32_t> last_sleep_duration_;
};

}  // namespace deep_sleep
}  // namespace esphome

#endif

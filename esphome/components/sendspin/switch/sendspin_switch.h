#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/switch/switch.h"

namespace esphome::sendspin_ {

/// @brief Switch that starts and stops the whole Sendspin client through the hub.
///
/// Off stops the server, the roles and the mDNS advertisement; on brings them back. The hub
/// always starts its client in setup(), so a restored off state is applied here afterwards.
class SendspinSwitch final : public switch_::Switch, public SendspinChild {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::sendspin_

#endif  // USE_ESP32

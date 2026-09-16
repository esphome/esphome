#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/switch/switch.h"

namespace esphome::sendspin_ {

/// @brief Switch that starts and stops the whole Sendspin client through the hub.
///
/// Off stops the server, the roles and the mDNS advertisement; on brings them back. Sets up
/// before the hub so the restored state decides whether the hub's setup() starts the client.
class SendspinSwitch final : public switch_::Switch, public Component, public Parented<SendspinHub> {
 public:
  float get_setup_priority() const override { return sendspin_priority::ENABLE_SWITCH; }
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::sendspin_

#endif  // USE_ESP32

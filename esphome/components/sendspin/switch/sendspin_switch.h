#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_SWITCH)

#include "esphome/components/sendspin/sendspin_hub.h"
#include "esphome/components/switch/switch.h"

namespace esphome::sendspin_ {

/// @brief Switch that starts and stops the Sendspin client through the hub (see SendspinHub::set_enabled()).
class SendspinEnabledSwitch final : public switch_::Switch, public SendspinChild {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

/// @brief Switch that turns unpaired (Sentinel) access on and off through the hub (see
/// SendspinHub::set_unpaired_access_enabled()). The library does not persist the setting, so the switch's restore
/// mode does.
class SendspinUnpairedAccessSwitch final : public switch_::Switch, public SendspinChild {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace esphome::sendspin_

#endif

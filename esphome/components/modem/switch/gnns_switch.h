#pragma once
#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_SWITCH

#include "esphome/core/component.h"
#include "../modem_component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace modem {

class GnssSwitch : public switch_::Switch, public Component {
 public:
  void set_command(const std::string &command) { this->command_ = command; }
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

  void dump_config() override;
  void setup() override;
  void loop() override;

 protected:
  std::string command_;
  void write_state(bool state) override;
  bool modem_state_{false};
};

}  // namespace modem
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_SWITCH
#endif  // USE_ESP_IDF

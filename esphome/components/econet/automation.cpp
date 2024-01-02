#include "esphome/core/log.h"

#include "automation.h"

static const char *const TAG = "econet.automation";

namespace esphome {
namespace econet {

EconetRawDatapointUpdateTrigger::EconetRawDatapointUpdateTrigger(Econet *parent, const std::string &sensor_id,
                                                                 int8_t request_mod, bool request_once,
                                                                 uint32_t src_adr) {
  parent->register_listener(
      sensor_id, request_mod, request_once, [this](const EconetDatapoint &dp) { this->trigger(dp.value_raw); }, true,
      src_adr);
}

}  // namespace econet
}  // namespace esphome

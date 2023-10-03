#include "esphome/core/log.h"

#include "automation.h"

static const char *const TAG = "econet.automation";

namespace esphome {
namespace econet {

EconetRawDatapointUpdateTrigger::EconetRawDatapointUpdateTrigger(Econet *parent, const std::string &sensor_id,
                                                                 int8_t request_mod) {
  parent->register_listener(
      sensor_id, request_mod, [this](const EconetDatapoint &dp) { this->trigger(dp.value_raw); }, true);
}

}  // namespace econet
}  // namespace esphome

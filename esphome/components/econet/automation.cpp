#include "esphome/core/log.h"

#include "automation.h"

static const char *const TAG = "econet.automation";

namespace esphome {
namespace econet {

EconetRawDatapointUpdateTrigger::EconetRawDatapointUpdateTrigger(Econet *parent, const std::string &sensor_id) {
  // RAW datapoints need to be requested separately.
  // For now rely on other devices, e.g. thermostat, requesting them.
  bool listen_only = true;
  parent->register_listener(sensor_id, listen_only, [this](const EconetDatapoint &dp) { this->trigger(dp.value_raw); });
}

}  // namespace econet
}  // namespace esphome

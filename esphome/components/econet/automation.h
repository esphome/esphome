#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "econet.h"

#include <vector>

namespace esphome {
namespace econet {

class EconetRawDatapointUpdateTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit EconetRawDatapointUpdateTrigger(Econet *parent, const std::string &sensor_id, int8_t request_mod);
};

}  // namespace econet
}  // namespace esphome

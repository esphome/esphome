#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "econet.h"

#include <vector>

namespace esphome {
namespace econet {

class EconetRawDatapointUpdateTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit EconetRawDatapointUpdateTrigger(Econet *parent, const std::string &sensor_id, int8_t request_mod,
                                           bool request_once, uint32_t src_adr);
};

}  // namespace econet
}  // namespace esphome

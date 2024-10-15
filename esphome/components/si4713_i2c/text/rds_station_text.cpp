#include "rds_station_text.h"

namespace esphome {
namespace si4713 {

void RDSStationText::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_rds_station(value);
}

}  // namespace si4713
}  // namespace esphome

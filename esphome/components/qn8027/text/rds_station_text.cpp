#include "rds_station_text.h"

namespace esphome {
namespace qn8027 {

void RDSStationText::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_rds_station(value);
}

}  // namespace qn8027
}  // namespace esphome

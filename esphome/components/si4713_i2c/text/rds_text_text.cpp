#include "rds_text_text.h"

namespace esphome {
namespace si4713 {

void RDSTextText::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_rds_text(value);
}

}  // namespace si4713
}  // namespace esphome

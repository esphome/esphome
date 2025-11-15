#include "query_button.h"

namespace esphome {
namespace ld2412 {

void QueryButton::press_action() { this->parent_->read_all_info(); }

}  // namespace ld2412
}  // namespace esphome

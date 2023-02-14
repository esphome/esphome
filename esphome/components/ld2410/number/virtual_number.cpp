#include "virtual_number.h"

namespace esphome {
namespace ld2410 {

VirtualNumber::VirtualNumber() {}

void VirtualNumber::control(float value) { this->publish_state(value); }

}  // namespace ld2410
}  // namespace esphome

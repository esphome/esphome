#include "ld2410_number.h"

namespace esphome {
namespace ld2410 {

LD2410Number::LD2410Number() {}

void LD2410Number::control(float value) { this->publish_state(value); }

}  // namespace ld2410
}  // namespace esphome

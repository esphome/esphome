#include "ld2410_select.h"

namespace esphome {
namespace ld2410 {

LD2410Select::LD2410Select() {}

void LD2410Select::control(const std::string &value) { this->publish_state(value); }

}  // namespace ld2410
}  // namespace esphome

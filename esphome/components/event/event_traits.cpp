#include "event_traits.h"

namespace esphome {
namespace event {

void EventTraits::set_types(std::vector<std::string> types) { this->types_ = std::move(types); }

std::vector<std::string> EventTraits::get_types() const { return this->types_; }

}  // namespace event
}  // namespace esphome

#include "select_traits.h"

namespace esphome {
namespace select {

void SelectTraits::set_options(const std::initializer_list<std::string> &options) { this->options_ = options; }

const FixedVector<std::string> &SelectTraits::get_options() const { return this->options_; }

}  // namespace select
}  // namespace esphome

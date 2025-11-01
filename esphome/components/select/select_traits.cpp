#include "select_traits.h"

namespace esphome {
namespace select {

void SelectTraits::set_options(const std::initializer_list<const char *> &options) { this->options_ = options; }

void SelectTraits::set_options(const FixedVector<const char *> &options) {
  this->options_.init(options.size());
  for (size_t i = 0; i < options.size(); i++) {
    this->options_[i] = options[i];
  }
}

const FixedVector<const char *> &SelectTraits::get_options() const { return this->options_; }

}  // namespace select
}  // namespace esphome

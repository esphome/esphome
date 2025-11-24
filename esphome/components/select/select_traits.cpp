#include "select_traits.h"

namespace esphome::select {

void SelectTraits::set_options(const std::initializer_list<const char *> &options) { this->options_ = options; }

void SelectTraits::set_options(const FixedVector<const char *> &options) {
  this->options_.init(options.size());
  for (const auto &opt : options) {
    this->options_.push_back(opt);
  }
}

const FixedVector<const char *> &SelectTraits::get_options() const { return this->options_; }

}  // namespace esphome::select

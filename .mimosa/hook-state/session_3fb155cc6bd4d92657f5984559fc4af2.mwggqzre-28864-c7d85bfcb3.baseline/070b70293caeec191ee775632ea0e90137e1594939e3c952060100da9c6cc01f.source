#include "select_traits.h"

namespace esphome::select {

// Runtime option lists are copied, since the argument may not outlive the select; one
// out of line copy keeps a single instance of the copy code.
void SelectTraits::set_options_copy_(const char *const *options, size_t count) {
  this->options_.assign_copy(options, count);
}

void SelectTraits::set_options(const std::initializer_list<const char *> &options) {
  this->set_options_copy_(options.begin(), options.size());
}

void SelectTraits::set_options(const FixedVector<const char *> &options) {
  this->set_options_copy_(options.begin(), options.size());
}

}  // namespace esphome::select

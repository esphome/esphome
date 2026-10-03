#include "select_traits.h"

#include <algorithm>

namespace esphome::select {

// Runtime option lists are copied, since the argument may not outlive the select; the previous
// copy is freed, flash tables never are.
void SelectTraits::set_options_copy_(const char *const *options, size_t count) {
  auto *table = new const char *[count];  // NOLINT(cppcoreguidelines-owning-memory)
  std::copy(options, options + count, table);
  if (this->options_.size_ & SelectOptions::OWNED_BIT)
    delete[] this->options_.data_;  // NOLINT(cppcoreguidelines-owning-memory)
  this->options_ = SelectOptions(table, count | SelectOptions::OWNED_BIT);
}

void SelectTraits::set_options(const std::initializer_list<const char *> &options) {
  this->set_options_copy_(options.begin(), options.size());
}

void SelectTraits::set_options(const FixedVector<const char *> &options) {
  this->set_options_copy_(options.begin(), options.size());
}

}  // namespace esphome::select

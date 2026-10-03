#include "select_traits.h"

namespace esphome::select {

// The deprecated overloads copy, since their argument may not outlive the select.
static SelectOptions copy_options(const char *const *options, size_t count) {
  auto *table = new const char *[count];  // NOLINT(cppcoreguidelines-owning-memory)
  std::copy(options, options + count, table);
  return SelectOptions(table, count);
}

void SelectTraits::set_options(const std::initializer_list<const char *> &options) {
  this->options_ = copy_options(options.begin(), options.size());
}

void SelectTraits::set_options(const FixedVector<const char *> &options) {
  this->options_ = copy_options(options.begin(), options.size());
}

}  // namespace esphome::select

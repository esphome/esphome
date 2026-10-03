#include "select_traits.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace esphome::select {

// The deprecated overloads copy, since their argument may not outlive the select. Copies are
// tracked per select so a repeated call frees the previous one; only these overloads use the list.
struct OwnedOptions {
  const SelectTraits *traits;
  std::unique_ptr<const char *[]> table;
};
static std::vector<OwnedOptions> &owned_options() {
  static std::vector<OwnedOptions> owned;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  return owned;
}

void SelectTraits::set_options_copy_(const char *const *options, size_t count) {
  auto table = std::make_unique<const char *[]>(count);
  std::copy(options, options + count, table.get());
  this->options_ = SelectOptions(table.get(), count);
  for (auto &owned : owned_options()) {
    if (owned.traits == this) {
      owned.table = std::move(table);
      return;
    }
  }
  owned_options().push_back({this, std::move(table)});
}

void SelectTraits::set_options(const std::initializer_list<const char *> &options) {
  this->set_options_copy_(options.begin(), options.size());
}

void SelectTraits::set_options(const FixedVector<const char *> &options) {
  this->set_options_copy_(options.begin(), options.size());
}

}  // namespace esphome::select

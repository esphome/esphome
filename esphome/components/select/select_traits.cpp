#include "select_traits.h"

namespace esphome {
namespace select {

void SelectTraits::set_options(std::vector<std::string> options) { this->options_ = std::move(options); }

std::vector<std::string> SelectTraits::get_options() const { return this->options_; }

}  // namespace select
}  // namespace esphome

#include "select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace select {

static const char *const TAG = "select";

void SelectCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  if (!this->option_.has_value()) {
    ESP_LOGW(TAG, "No value set for SelectCall");
    return;
  }

  const auto &traits = this->parent_->traits;
  auto value = *this->option_;
  auto options = traits.get_options();

  if (std::find(options.begin(), options.end(), value) == options.end()) {
    ESP_LOGW(TAG, "  Option %s is not a valid option.", value.c_str());
    return;
  }

  ESP_LOGD(TAG, "  Option: %s", (*this->option_).c_str());
  this->parent_->control(*this->option_);
}

void Select::publish_state(const std::string &state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %s", this->get_name().c_str(), state.c_str());
  this->state_callback_.call(state);
}

void Select::add_on_state_callback(std::function<void(std::string)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

uint32_t Select::hash_base() { return 2812997003UL; }

}  // namespace select
}  // namespace esphome

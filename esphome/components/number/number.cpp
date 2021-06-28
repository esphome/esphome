#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

void NumberCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  if (this->value_.has_value()) {
    auto value = *this->value_;
    uint8_t accuracy = this->parent_->get_accuracy_decimals();
    float min_value = this->parent_->min_value();
    if (value < min_value) {
      ESP_LOGW(TAG, "  Value %s must not be less than minimum %s", value_accuracy_to_string(value, accuracy).c_str(), value_accuracy_to_string(min_value, accuracy).c_str());
      this->value_.reset();
      return;
    }
    float max_value = this->parent_->max_value();
    if (value > max_value) {
      ESP_LOGW(TAG, "  Value %s must not be larger than maximum %s", value_accuracy_to_string(value, accuracy).c_str(), value_accuracy_to_string(min_value, accuracy).c_str());
      this->value_.reset();
      return;
    }
    ESP_LOGD(TAG, "  Value: %s", value_accuracy_to_string(*this->value_, accuracy).c_str());
    this->parent_->set(*this->value_);
  }
}

NumberCall &NumberCall::set_value(float value) {
  this->value_ = value;
  return *this;
}

const optional<float> &NumberCall::get_value() const { return this->value_; }

NumberCall Number::make_call() { return NumberCall(this); }

void Number::publish_state(float state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %.5f", this->get_name().c_str(), state);
  this->state_callback_.call(state);
}

uint32_t Number::update_interval() { return 0; }
Number::Number(const std::string &name) : Nameable(name), state(NAN) {}
Number::Number() : Number("") {}

void Number::add_on_state_callback(std::function<void(float)> &&callback) { this->state_callback_.add(std::move(callback)); }
void Number::set_icon(const std::string &icon) { this->icon_ = icon; }
std::string Number::get_icon() {
  return *this->icon_;
}
int8_t Number::get_accuracy_decimals() {
  // use printf %g to find number of digits based on step
  char buf[32];
  sprintf(buf, "%.5g", this->step_);
  std::string str{buf};
  size_t dot_pos = str.find('.');
  if (dot_pos == std::string::npos)
    return 0;

  return str.length() - dot_pos - 1;
}
float Number::get_state() const { return this->state; }

bool Number::has_state() const { return this->has_state_; }

uint32_t Number::hash_base() { return 2282307003UL; }

}  // namespace number
}  // namespace esphome

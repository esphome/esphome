#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

void Number::publish_state(float state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %.5f", this->get_name().c_str(), state);
  this->callback_.call(state);
}
std::string Number::icon() { return ""; }
uint32_t Number::update_interval() { return 0; }
Number::Number(const std::string &name) : Nameable(name), state(NAN) {}
Number::Number() : Number("") {}

void Number::set_icon(const std::string &icon) { this->icon_ = icon; }
void Number::add_on_state_callback(std::function<void(float)> &&callback) { this->callback_.add(std::move(callback)); }
std::string Number::get_icon() {
  if (this->icon_.has_value())
    return *this->icon_;
  return this->icon();
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
std::string Number::unique_id() { return ""; }

bool Number::has_state() const { return this->has_state_; }

uint32_t Number::hash_base() { return 2282307003UL; }

PollingNumberComponent::PollingNumberComponent(const std::string &name, uint32_t update_interval)
    : PollingComponent(update_interval), Number(name) {}

uint32_t PollingNumberComponent::update_interval() { return this->get_update_interval(); }

}  // namespace number
}  // namespace esphome

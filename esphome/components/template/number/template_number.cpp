#include "template_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.number";

void TemplateNumber::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (val.has_value()) {
    auto call = this->make_call();
    call.set_value(*val);
    call.perform();
  }
}

void TemplateNumber::set(const float value) {
  // Does nothing specific for template number.
  this->publish_state(value);
}
float TemplateNumber::get_setup_priority() const { return setup_priority::HARDWARE; }
void TemplateNumber::set_template(std::function<optional<float>()> &&f) { this->f_ = f; }
void TemplateNumber::dump_config() {
  LOG_NUMBER("", "Template Number", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome

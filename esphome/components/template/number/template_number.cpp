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
    this->publish_state(*val);
  }
}
float TemplateNumber::get_setup_priority() const { return setup_priority::HARDWARE; }
void TemplateNumber::set_template(std::function<optional<float>()> &&f) { this->f_ = f; }
void TemplateNumber::dump_config() {
  LOG_NUMBER("", "Template Number", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome

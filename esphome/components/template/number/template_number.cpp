#include "template_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.number";

TemplateNumber::TemplateNumber() : set_trigger_(new Trigger<float>()) {}

void TemplateNumber::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (val.has_value()) {
    this->publish_state(*val);
  }
}

void TemplateNumber::set(float value) {
  this->set_trigger_->trigger(value);

  if (this->optimistic_)
    this->publish_state(value);
}
float TemplateNumber::get_setup_priority() const { return setup_priority::HARDWARE; }
void TemplateNumber::set_template(std::function<optional<float>()> &&f) { this->f_ = f; }
void TemplateNumber::dump_config() {
  LOG_NUMBER("", "Template Number", this);
  LOG_UPDATE_INTERVAL(this);
}

void TemplateNumber::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
Trigger<float> *TemplateNumber::get_set_trigger() const { return this->set_trigger_; };

}  // namespace template_
}  // namespace esphome

#include "template_text.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.text";

void TemplateText::setup() {
  if (!(this->f_ == nullptr)) {
    if (this->f_.has_value())
      return;
  }

  std::string value;
  ESP_LOGD(TAG, "Setting up Template Text Input");
  value = this->initial_value_;
  if (!this->pref_) {
    ESP_LOGD(TAG, "State from initial: %s", value.c_str());
  } else {
    this->pref_->setup(this->get_object_id_hash(), value);
  }
  this->publish_state(value);
}

void TemplateText::update() {
  if (this->f_ == nullptr)
    return;

  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);
}

void TemplateText::control(const std::string &value) {
  this->set_trigger_->trigger(value);

  if (this->optimistic_)
    this->publish_state(value);

  if (this->pref_) {
    if (!this->pref_->save(value)) {
      ESP_LOGW(TAG, "Text value too long to save");
    }
  }
}
void TemplateText::dump_config() {
  LOG_TEXT("", "Template Text Input", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome

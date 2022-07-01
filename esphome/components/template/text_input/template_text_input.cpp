#include "template_text_input.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.text_input";

void TemplateTextInput::setup() {
  if (this->f_.has_value())
    return;

  std::string value;
  ESP_LOGD(TAG, "Setting up Template Text Input");
  if (!this->restore_value_) {
    value = this->initial_value_;
    ESP_LOGD(TAG, "State from initial: %s", value.c_str());
  } else {
    value = "";
/*    this->pref_ = global_preferences->make_preference<uint32_t>(this->get_object_id_hash());
    if (!this->pref_.load(&value)) {
      if (!std::isnan(this->initial_value_)) {
        value = this->initial_value_;
      } else {
        value = "";
      }
    }
*/
  }
  this->publish_state(value);
}

void TemplateTextInput::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);
}

void TemplateTextInput::control(std::string &value) {
  this->set_trigger_->trigger(value);

  if (this->optimistic_)
    this->publish_state(value);

  if (this->restore_value_)
    this->pref_.save(&value);
}
void TemplateTextInput::dump_config() {
  LOG_TEXT_INPUT("", "Template Text Input", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome

#include "template_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.select";

void TemplateSelect::setup() {
  if (this->f_.has_value())
    return;

  std::string value;
  ESP_LOGD(TAG, "Setting up Template Number");
  if (!this->restore_value_) {
    if (this->initial_value_.has_value()) {
      value = *this->initial_value_;
      ESP_LOGD(TAG, "State from initial: %s", value.c_str());
    } else {
      value = this->traits.get_options().front();
      ESP_LOGD(TAG, "State from options: %s", value.c_str());
    }
  } else {
    this->pref_ = global_preferences.make_preference<std::string>(this->get_object_id_hash());
    if (!this->pref_.load(&value) || value.empty()) {
      if (this->initial_value_.has_value()) {
        value = *this->initial_value_;
        ESP_LOGD(TAG, "State from initial (could not load): %s", value.c_str());
      } else {
        value = this->traits.get_options().front();
        ESP_LOGD(TAG, "State from options (could not load): %s", value.c_str());
      }
    } else {
      ESP_LOGD(TAG, "State from restore: %s", value.c_str());
    }
  }

  this->publish_state(value);
}

void TemplateSelect::update() {
  if (!this->f_.has_value())
    return;

  auto val = (*this->f_)();
  if (!val.has_value())
    return;

  this->publish_state(*val);
}

void TemplateSelect::control(const std::string &value) {
  this->set_trigger_->trigger(value);

  if (this->optimistic_)
    this->publish_state(value);

  if (this->restore_value_)
    this->pref_.save(&value);
}
void TemplateSelect::dump_config() {
  LOG_SELECT("", "Template Select", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace template_
}  // namespace esphome

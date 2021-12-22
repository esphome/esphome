#include "template_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.select";

void TemplateSelect::setup() {
  if (this->f_.has_value())
    return;

  std::string value;
  ESP_LOGD(TAG, "Setting up Template Select");
  if (!this->restore_value_) {
    value = this->initial_option_;
    ESP_LOGD(TAG, "State from initial: %s", value.c_str());
  } else {
    size_t index;
    this->pref_ = global_preferences->make_preference<size_t>(this->get_object_id_hash());
    if (!this->pref_.load(&index)) {
      value = this->initial_option_;
      ESP_LOGD(TAG, "State from initial (could not load): %s", value.c_str());
    } else {
      value = this->traits.get_options().at(index);
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

  auto options = this->traits.get_options();
  if (std::find(options.begin(), options.end(), *val) == options.end()) {
    ESP_LOGE(TAG, "lambda returned an invalid option %s", (*val).c_str());
    return;
  }

  this->publish_state(*val);
}

void TemplateSelect::control(const std::string &value) {
  this->set_trigger_->trigger(value);

  if (this->optimistic_)
    this->publish_state(value);

  if (this->restore_value_) {
    auto options = this->traits.get_options();
    size_t index = std::find(options.begin(), options.end(), value) - options.begin();

    this->pref_.save(&index);
  }
}
void TemplateSelect::dump_config() {
  LOG_SELECT("", "Template Select", this);
  LOG_UPDATE_INTERVAL(this);
  if (this->f_.has_value())
    return;
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
  ESP_LOGCONFIG(TAG, "  Initial Option: %s", this->initial_option_.c_str());
  ESP_LOGCONFIG(TAG, "  Restore Value: %s", YESNO(this->restore_value_));
}

}  // namespace template_
}  // namespace esphome

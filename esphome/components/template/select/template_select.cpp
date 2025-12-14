#include "template_select.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.select";

void TemplateSelect::setup() {
  if (this->f_.has_value())
    return;

  size_t index = this->initial_option_index_;
  if (this->restore_value_) {
    this->pref_ = global_preferences->make_preference<size_t>(this->get_preference_hash());
    size_t restored_index;
    if (this->pref_.load(&restored_index) && this->has_index(restored_index)) {
      index = restored_index;
      ESP_LOGD(TAG, "State from restore: %s", this->option_at(index));
    } else {
      ESP_LOGD(TAG, "State from initial (could not load or invalid stored index): %s", this->option_at(index));
    }
  } else {
    ESP_LOGD(TAG, "State from initial: %s", this->option_at(index));
  }

  this->publish_state(index);
}

void TemplateSelect::update() {
  if (!this->f_.has_value())
    return;

  auto val = this->f_();
  if (val.has_value()) {
    if (!this->has_option(*val)) {
      ESP_LOGE(TAG, "Lambda returned an invalid option: %s", (*val).c_str());
      return;
    }
    this->publish_state(*val);
  }
}

void TemplateSelect::control(size_t index) {
  this->set_trigger_->trigger(std::string(this->option_at(index)));

  if (this->optimistic_)
    this->publish_state(index);

  if (this->restore_value_)
    this->pref_.save(&index);
}

void TemplateSelect::dump_config() {
  LOG_SELECT("", "Template Select", this);
  LOG_UPDATE_INTERVAL(this);
  if (this->f_.has_value())
    return;
  ESP_LOGCONFIG(TAG,
                "  Optimistic: %s\n"
                "  Initial Option: %s\n"
                "  Restore Value: %s",
                YESNO(this->optimistic_), this->option_at(this->initial_option_index_), YESNO(this->restore_value_));
}

}  // namespace esphome::template_

#include "template_text.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.text";

void TemplateText::setup() {
  if (this->f_.has_value())
    return;
  std::string value = this->initial_value_;
  if (!this->pref_) {
    ESP_LOGD(TAG, "State from initial: %s", value.c_str());
  } else {
    uint32_t key = this->get_preference_hash();
    key += this->traits.get_min_length() << 2;
    key += this->traits.get_max_length() << 4;
    key += fnv1_hash(this->traits.get_pattern_c_str()) << 6;
    this->pref_->setup(key, value);
  }
  if (!value.empty())
    this->publish_state(value);
}

void TemplateText::update() {
  if (!this->f_.has_value())
    return;

  auto val = this->f_();
  if (val.has_value()) {
    this->publish_state(*val);
  }
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

}  // namespace esphome::template_

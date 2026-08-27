#include "template_text.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.text";

void TemplateText::setup() {
  if (this->f_.has_value())
    return;

  if (this->pref_ == nullptr) {
    // No restore - use const char* directly, no heap allocation needed
    if (this->initial_value_ != nullptr && this->initial_value_[0] != '\0') {
      ESP_LOGD(TAG, "State from initial: %s", this->initial_value_);
      this->publish_state(this->initial_value_);
    }
    return;
  }

  // Need std::string for pref_->setup() to fill from flash
  std::string value{this->initial_value_ != nullptr ? this->initial_value_ : ""};
  uint32_t extra = 0;
  extra += this->traits.get_min_length() << 2;
  extra += this->traits.get_max_length() << 4;
  extra += fnv1_hash(this->traits.get_pattern_c_str()) << 6;
  // TextSaver::setup() picks the key for the platform and migrates old data once
  uint32_t key = this->preference_key_base_() + extra;
  uint32_t old_key = this->old_preference_key_base_() + extra;
  this->pref_->setup(key, old_key, value);
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
  this->set_trigger_.trigger(value);

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

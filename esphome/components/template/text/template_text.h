#pragma once

#include "esphome/components/text/text.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/template_lambda.h"

namespace esphome::template_ {

// We keep this separate so we don't have to template and duplicate
// the text input for each different size flash allocation.
class TemplateTextSaverBase {
 public:
  virtual bool save(const std::string &value) { return true; }

  /// old_id is the pre-2026.8.0 preference key; data stored under it is moved to id once.
  /// See: https://github.com/esphome/backlog/issues/85
  virtual void setup(uint32_t id, uint32_t old_id, std::string &value) {}

 protected:
  ESPPreferenceObject pref_;
  std::string prev_;
};

template<uint8_t SZ> class TextSaver : public TemplateTextSaverBase {
 public:
  bool save(const std::string &value) override {
    if (value == this->prev_) {
      return true;  // No change, nothing to save
    }
    // If string is bigger than the allocation, do not save it.
    // We don't need to waste ram setting prev_value either.
    int size = value.size();
    if (size > SZ) {
      return false;
    }
    // Make it into a length prefixed thing
    unsigned char temp[SZ + 1];
    memcpy(temp + 1, value.c_str(), size);
    // SZ should be pre checked at the schema level, it can't go past the char range.
    temp[0] = ((unsigned char) size);
    this->pref_.save(&temp);
    this->prev_.assign(value);
    return true;
  }

  // Make the preference object.  Fill the provided location with the saved data
  // If it is available, else leave it alone
  void setup(uint32_t id, uint32_t old_id, std::string &value) override {
    char temp[SZ + 1];
#ifdef USE_PREFERENCE_KEY_LOOKUP
    this->pref_ = global_preferences->make_preference<uint8_t[SZ + 1]>(id);
    bool hasdata = migrate_preference(this->pref_, reinterpret_cast<uint8_t *>(temp), SZ + 1, old_id, id);
#else
    // Slot-based backends keep the old key; it is only a validity tag on a positional slot
    this->pref_ = global_preferences->make_preference<uint8_t[SZ + 1]>(old_id);
    bool hasdata = this->pref_.load(&temp);
#endif

    if (hasdata) {
      size_t len = static_cast<uint8_t>(temp[0]);
      if (len > SZ) {
        len = SZ;
      }
      value.assign(temp + 1, len);
    }

    this->prev_.assign(value);
  }
};

class TemplateText final : public text::Text, public PollingComponent {
 public:
  template<typename F> void set_template(F &&f) { this->f_.set(std::forward<F>(f)); }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  Trigger<std::string> *get_set_trigger() { return &this->set_trigger_; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_initial_value(const char *initial_value) { this->initial_value_ = initial_value; }
  /// Prevent accidental use of std::string which would dangle
  void set_initial_value(const std::string &initial_value) = delete;
  void set_value_saver(TemplateTextSaverBase *restore_value_saver) { this->pref_ = restore_value_saver; }

 protected:
  void control(const std::string &value) override;
  bool optimistic_ = false;
  const char *initial_value_{nullptr};
  Trigger<std::string> set_trigger_;
  TemplateLambda<std::string> f_{};

  TemplateTextSaverBase *pref_ = nullptr;
};

}  // namespace esphome::template_

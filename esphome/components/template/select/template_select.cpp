#include "template_select.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.select";

void dump_config_helper(BaseTemplateSelect *sel_comp, bool optimistic, bool has_lambda,
                        const size_t initial_option_index, bool restore_value) {
  LOG_SELECT("", "Template Select", sel_comp);
  if (has_lambda) {
    LOG_UPDATE_INTERVAL(sel_comp);
  } else {
    ESP_LOGCONFIG(TAG,
                  "  Optimistic: %s\n"
                  "  Initial Option: %s\n"
                  "  Restore Value: %s",
                  YESNO(optimistic), sel_comp->option_at(initial_option_index), YESNO(restore_value));
  }
}

void setup_initial(BaseTemplateSelect *sel_comp, size_t initial_index) {
  ESP_LOGD(TAG, "State from initial: %s", sel_comp->option_at(initial_index));
  sel_comp->publish_state(initial_index);
}

void setup_with_restore(BaseTemplateSelect *sel_comp, ESPPreferenceObject &pref, size_t initial_index) {
  size_t index = initial_index;
  if (pref.load(&index) && sel_comp->has_index(index)) {
    ESP_LOGD(TAG, "State from restore: %s", sel_comp->option_at(index));
  } else {
    index = initial_index;
    ESP_LOGD(TAG, "State from initial (no valid stored index): %s", sel_comp->option_at(initial_index));
  }
  sel_comp->publish_state(index);
}

void update_lambda(BaseTemplateSelect *sel_comp, const optional<std::string> &val) {
  if (val.has_value()) {
    if (!sel_comp->has_option(*val)) {
      ESP_LOGE(TAG, "Lambda returned an invalid option: %s", (*val).c_str());
      return;
    }
    sel_comp->publish_state(*val);
  }
}

}  // namespace esphome::template_

#include "template_select.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

void dump_config_helper(BaseTemplateSelect *this_, bool optimistic, bool has_lambda, const size_t initial_option_index,
                        bool restore_value) {
  LOG_SELECT("", "Template Select", this_);
  if (has_lambda) {
    LOG_UPDATE_INTERVAL((PollingComponent *) this_);
  } else {
    ESP_LOGCONFIG(TAG,
                  "  Optimistic: %s\n"
                  "  Initial Option: %s\n"
                  "  Restore Value: %s",
                  YESNO(optimistic), this_->option_at(initial_option_index), YESNO(restore_value));
  }
}
}  // namespace esphome::template_

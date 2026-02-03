#include "template_select.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

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
}  // namespace esphome::template_

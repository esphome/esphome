#include "esphome/core/defines.h"

#if defined(USE_MIDEA_PURIFIER) && \
    ((defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF))
#include "purifier_switch.h"
#include "air_conditioner.h"
#include "ac_adapter.h"
#include "esphome/core/log.h"

namespace esphome::midea::ac {

void PurifierSwitch::dump_config() {
  switch_::log_switch(Constants::TAG, "  ", LOG_STR_LITERAL("Purifier"), this);
  ESP_LOGCONFIG(Constants::TAG, "  Status max age: %" PRIu32 "ms", this->status_max_age_);
}

void PurifierSwitch::write_state(bool state) {
  if (!this->parent_->set_purifier(state, this->status_max_age_)) {
    ESP_LOGD(Constants::TAG,
             "Purifier request not queued (status unknown/stale, control busy, or already in requested state)");
  }
}

}  // namespace esphome::midea::ac
#endif

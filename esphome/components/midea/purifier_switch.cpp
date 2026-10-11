#include "esphome/core/defines.h"

#if defined(USE_MIDEA_PURIFIER) && \
    ((defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF))
#include "purifier_switch.h"
#include "air_conditioner.h"
#include "esphome/core/log.h"

namespace esphome::midea::ac {

void AirConditioner::set_purifier_switch(PurifierSwitch *purifier) {
  this->base_.addOnPurifierStatusCallback([purifier](bool state) { purifier->publish_state(state); });
}

void PurifierSwitch::write_state(bool state) {
  if (!this->parent_->set_purifier(state, this->status_max_age_)) {
    ESP_LOGW("midea",
             "Purifier request not queued (status unknown/stale, control busy, or already in requested state)");
  }
}

}  // namespace esphome::midea::ac
#endif

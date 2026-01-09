#include "nextion.h"

#ifdef USE_NEXTION_TFT_UPLOAD

#include "esphome/core/application.h"

namespace esphome {
namespace nextion {
static const char *const TAG = "nextion.upload";

bool Nextion::upload_end_(bool successful) {
  if (successful) {
    ESP_LOGD(TAG, "Upload successful");
    for (uint8_t i = 0; i <= 5; i++) {
      delay(1000);     // NOLINT
      App.feed_wdt();  // Feed the watchdog timer.
    }
    App.safe_reboot();
  } else {
    ESP_LOGE(TAG, "Upload failed");

    this->connection_state_.is_updating_ = false;
    this->connection_state_.ignore_is_setup_ = false;

    uint32_t baud_rate = this->parent_->get_baud_rate();
    if (baud_rate != this->original_baud_rate_) {
      ESP_LOGD(TAG, "Baud: %" PRIu32 "->%" PRIu32, baud_rate, this->original_baud_rate_);
      this->parent_->set_baud_rate(this->original_baud_rate_);
      this->parent_->load_settings();
    }
  }

  return successful;
}

}  // namespace nextion
}  // namespace esphome

#endif  // USE_NEXTION_TFT_UPLOAD

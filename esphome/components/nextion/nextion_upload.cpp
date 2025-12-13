#include "nextion.h"

#ifdef USE_NEXTION_TFT_UPLOAD

#include "nextion_upload.h"

#include "esphome/components/network/util.h"  // For network::is_connected()
#include "esphome/core/application.h"         // For App
#include "esphome/core/log.h"                 // For ESP_LOGW, ESP_LOGE, ESP_LOGD

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

bool Nextion::upload_validate_and_prepare_(bool exit_reparse) {
  if (this->connection_state_.is_updating_) {
    ESP_LOGW(TAG, "Upload in progress");
    return false;
  }

  if (!network::is_connected()) {
    ESP_LOGE(TAG, "No network");
    return false;
  }

  this->connection_state_.is_updating_ = true;

  if (exit_reparse) {
    ESP_LOGD(TAG, "Exit reparse mode");
    if (!this->set_protocol_reparse_mode(false)) {
      ESP_LOGW(TAG, "Exit reparse failed");
      return false;
    }
  }

  return true;
}

uint32_t Nextion::upload_setup_baud_rate_(uint32_t baud_rate) {
  this->original_baud_rate_ = this->parent_->get_baud_rate();
  if (baud_rate <= 0) {
    baud_rate = this->original_baud_rate_;
  }
  ESP_LOGD(TAG, "Baud rate: %" PRIu32, baud_rate);
  return baud_rate;
}

bool Nextion::upload_prepare_nextion_(uint32_t baud_rate) {
  // The Nextion will ignore the upload command if it is sleeping
  ESP_LOGV(TAG, "Wake-up");
  this->connection_state_.ignore_is_setup_ = true;
  this->send_command_("sleep=0");
  this->send_command_("dim=100");
  delay(250);  // NOLINT

  App.feed_wdt();
  char command[64];
  // Tells the Nextion the content length of the tft file and baud rate it will be sent at
  // Once the Nextion accepts the command it will wait until the file is successfully uploaded
  // If it fails for any reason a power cycle of the display will be needed
  snprintf(command, sizeof(command), "whmi-wris %" PRIu32 ",%" PRIu32 ",1", this->content_length_, baud_rate);

  // Clear serial receive buffer
  ESP_LOGV(TAG, "Clear RX buffer");
  this->reset_(false);
  delay(250);  // NOLINT

  ESP_LOGV(TAG, "Upload cmd: %s", command);
  this->send_command_(command);

  if (baud_rate != this->original_baud_rate_) {
    ESP_LOGD(TAG, "Baud: %" PRIu32 "->%" PRIu32, this->original_baud_rate_, baud_rate);
    this->parent_->set_baud_rate(baud_rate);
    this->parent_->load_settings();
  }

  std::string response;
  ESP_LOGV(TAG, "Wait upload resp");
  this->recv_ret_string_(response, 5000, true);  // This can take some time to return

  // The Nextion display will, if it's ready to accept data, send a 0x05 byte.
  ESP_LOGD(TAG, "Upload resp: [%s] %zu B",
           format_hex_pretty(reinterpret_cast<const uint8_t *>(response.data()), response.size()).c_str(),
           response.length());

  if (response.find(0x05) != std::string::npos) {
    ESP_LOGV(TAG, "Upload prep done");
    return true;
  } else {
    ESP_LOGE(TAG, "Upload prep failed %d '%s'", response[0], response.c_str());
    return false;
  }
}

void build_range_header(char *buffer, size_t buffer_size, uint32_t range_start, uint32_t range_end) {
  snprintf(buffer, buffer_size, "bytes=%" PRIu32 "-%" PRIu32, range_start, range_end);
}

}  // namespace nextion
}  // namespace esphome

#endif  // USE_NEXTION_TFT_UPLOAD

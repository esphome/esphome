#include "pn7160.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::pn7160 {

static const char *const TAG = "pn7160";

void PN7160::setup() {
  if (this->dwl_req_pin_ != nullptr) {
    this->dwl_req_pin_->setup();
  }
  if (this->wkup_req_pin_ != nullptr) {
    this->wkup_req_pin_->setup();
  }
  PN71xx::setup();
}

void PN7160::dump_config() {
  ESP_LOGCONFIG(TAG, "PN7160:");
  PN71xx::dump_config();
  if (this->dwl_req_pin_ != nullptr) {
    LOG_PIN("  DWL_REQ pin: ", this->dwl_req_pin_);
  }
  if (this->wkup_req_pin_ != nullptr) {
    LOG_PIN("  WKUP_REQ pin: ", this->wkup_req_pin_);
  }
}

void PN7160::prepare_reset_() {
  // DWL_REQ must be low when VEN rises, or the chip starts in firmware download mode (UM11495)
  if (this->dwl_req_pin_ != nullptr) {
    this->dwl_req_pin_->digital_write(false);
    delay(pn71xx::NFCC_DEFAULT_TIMEOUT);
  }
}

uint8_t PN7160::verify_reset_(nfc::NciMessage &rx, const bool reset_config) {
  // PN7160 always sends CORE_RESET_NTF after CORE_RESET_RSP (UM11495, 8.2)
  if (this->read_nfcc(rx, pn71xx::NFCC_INIT_TIMEOUT) != nfc::STATUS_OK) {
    ESP_LOGE(TAG, "Reset notification was not received");
    return nfc::STATUS_FAILED;
  }
  if ((!rx.message_type_is(nfc::NCI_PKT_MT_CTRL_NOTIFICATION)) || (!rx.message_length_is(9)) ||
      (rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET] != 0x02) ||
      (rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 1] != (uint8_t) reset_config)) {
    char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
    ESP_LOGE(TAG, "Reset notification was malformed: %s", nfc::format_bytes_to(buf, rx.get_message()));
    return nfc::STATUS_FAILED;
  }

  // payload: trigger, config status, NCI version, manufacturer ID, info length (4), then the manufacturer specific
  // information: hardware, ROM code, FLASH major and FLASH minor versions (UM11495, 8.2 and 8.3)
  const auto &ntf = rx.get_message();
  ESP_LOGD(TAG,
           "Configuration %s, NCI version: 0x%02X, Manufacturer ID: 0x%02X\n"
           "  Hardware version: %u\n"
           "  ROM code version: %u\n"
           "  FLASH major version: %u\n"
           "  FLASH minor version: %u",
           ntf[4] ? LOG_STR_LITERAL("reset") : LOG_STR_LITERAL("retained"), ntf[5], ntf[6], ntf[8], ntf[9], ntf[10],
           ntf[11]);

  return nfc::STATUS_OK;
}

uint8_t PN7160::process_init_response_(nfc::NciMessage &rx) {
  // the chip's version information is logged from CORE_RESET_NTF in verify_reset_()
  if (rx.get_message().size() >= 8) {
    std::vector<uint8_t> features(rx.get_message().begin() + 4, rx.get_message().begin() + 8);
    char feat_buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
    ESP_LOGD(TAG, "NFCC features: %s", nfc::format_bytes_to(feat_buf, features));
  }

  return rx.get_simple_status_response();
}

}  // namespace esphome::pn7160

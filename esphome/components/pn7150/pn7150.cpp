#include "pn7150.h"

#include "esphome/core/log.h"

namespace esphome::pn7150 {

static const char *const TAG = "pn7150";

void PN7150::dump_config() {
  ESP_LOGCONFIG(TAG, "PN7150:");
  PN71xx::dump_config();
}

uint8_t PN7150::verify_reset_(nfc::NciMessage &rx, const bool reset_config) {
  // CORE_RESET_RSP payload: status, NCI version (reported as 1.1, see UM10936 4.1), configuration status
  if ((!rx.message_type_is(nfc::NCI_PKT_MT_CTRL_RESPONSE)) || (!rx.message_length_is(3)) ||
      (rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 1] != 0x11) ||
      (rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 2] != (uint8_t) reset_config)) {
    char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
    ESP_LOGE(TAG, "Reset response was malformed: %s", nfc::format_bytes_to(buf, rx.get_message()));
    return nfc::STATUS_FAILED;
  }

  ESP_LOGD(TAG, "Configuration %s, NCI version: 0x%02X",
           rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 2] ? LOG_STR_LITERAL("reset") : LOG_STR_LITERAL("retained"),
           rx.get_message()[nfc::NCI_PKT_PAYLOAD_OFFSET + 1]);

  return nfc::STATUS_OK;
}

uint8_t PN7150::process_init_response_(nfc::NciMessage &rx) {
  // NCI 1.0 CORE_INIT_RSP: the manufacturer ID and 4 bytes of manufacturer specific information follow the list of
  // supported RF interfaces, whose length is at offset 8 (UM10936, 5.2)
  const auto &msg = rx.get_message();
  if (msg.size() < 9u || msg.size() < 20u + msg[8]) {
    char buf[nfc::FORMAT_BYTES_BUFFER_SIZE];
    ESP_LOGE(TAG, "Initialise response too short: %s", nfc::format_bytes_to(buf, msg));
    return nfc::STATUS_FAILED;
  }
  const uint8_t n = msg[8];

  ESP_LOGD(TAG,
           "PN7150 chip info:\n"
           "  Manufacturer ID: 0x%02X\n"
           "  Hardware version: 0x%02X\n"
           "  ROM code version: 0x%02X\n"
           "  FLASH major version: 0x%02X\n"
           "  FLASH minor version: 0x%02X",
           msg[15 + n], msg[16 + n], msg[17 + n], msg[18 + n], msg[19 + n]);

  return rx.get_simple_status_response();
}

}  // namespace esphome::pn7150

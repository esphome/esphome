#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "esphome/core/component.h"

#include "tusb.h"
// tusb.h only pulls the HID class in when CFG_TUD_HID is set, which the static analysis build does not do
#include "class/hid/hid_device.h"

namespace esphome::tinyusb_keyboard {

static constexpr uint8_t REPORT_ID_KEYBOARD = 1;
static constexpr uint8_t REPORT_ID_CONSUMER = 2;
static constexpr uint8_t ENDPOINT_IN = 0x81;
static constexpr uint8_t POLL_INTERVAL_MS = 10;

// One HID interface carrying a keyboard and a consumer control (media keys), told apart by report id.
// Report ids rule out the boot protocol, so the interface declares no boot protocol, as TinyUSB's
// composite example does.
inline constexpr uint8_t HID_REPORT_DESCRIPTOR[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER)),
};

// esp_tinyusb ships default configuration descriptors for CDC, MSC and NCM only, so the keyboard supplies its own
inline constexpr uint8_t CONFIGURATION_DESCRIPTOR[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(HID_REPORT_DESCRIPTOR), ENDPOINT_IN, CFG_TUD_HID_EP_BUFSIZE,
                       POLL_INTERVAL_MS),
};

class TinyUSBKeyboard final : public Component {
 public:
  void setup() override { this->disable_loop(); }
  void loop() override;
  void dump_config() override;

  /// Press one key (HID usage id) with the given modifier bitmask; every other key is reported released.
  void press_key(uint8_t keycode, uint8_t modifiers) { this->set_keyboard_(keycode, modifiers); }
  void release_keys() { this->set_keyboard_(0, 0); }
  /// Press one consumer control usage, for example 0xE9 for volume up.
  void press_media(uint16_t usage) { this->set_consumer_(usage); }
  void release_media() { this->set_consumer_(0); }

 protected:
  void set_keyboard_(uint8_t keycode, uint8_t modifiers);
  void set_consumer_(uint16_t usage);
  /// Sends whatever changed; a report the endpoint cannot take yet is retried from loop()
  void flush_();

  // The wanted state of each report, resent until the endpoint accepts it so a release is never lost.
  // Only the latest state is kept, so consecutive key actions need a short delay between them.
  uint16_t usage_{0};
  uint8_t keycode_{0};
  uint8_t modifiers_{0};
  bool keyboard_pending_{false};
  bool consumer_pending_{false};
};

}  // namespace esphome::tinyusb_keyboard

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4

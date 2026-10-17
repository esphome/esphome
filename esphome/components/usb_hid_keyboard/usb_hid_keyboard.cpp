#ifdef USE_ZEPHYR
#include "usb_hid_keyboard.h"
#include "esphome/core/log.h"
#include <errno.h>
#include <zephyr/init.h>

namespace esphome::usb_hid_keyboard {

static const char *const TAG = "usb_hid_keyboard";

// Standard 8-byte boot-keyboard HID report descriptor (no report ID).
// Byte 0: modifier bitmask, byte 1: reserved, bytes 2-7: up to 6 key codes.
// Written as raw bytes to avoid SDK-version differences in HID_KEYBOARD_REPORT_DESC().
// clang-format off
static const uint8_t KBD_REPORT_DESC[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    // Modifier keys: 8 x 1-bit variable fields (Left/Right Ctrl/Shift/Alt/GUI)
    0x05, 0x07,        // Usage Page (Key Codes)
    0x19, 0xE0,        // Usage Minimum (0xE0 = Left Control)
    0x29, 0xE7,        // Usage Maximum (0xE7 = Right GUI)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x01,        // Logical Maximum (1)
    0x75, 0x01,        // Report Size (1 bit)
    0x95, 0x08,        // Report Count (8)
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    // Reserved byte: 1 x 8-bit constant variable field
    0x95, 0x01,        // Report Count (1)
    0x75, 0x08,        // Report Size (8 bits)
    0x81, 0x03,        // Input (Const, Variable, Absolute)
    // Key codes: 6 x 8-bit array fields
    0x95, 0x06,        // Report Count (6)
    0x75, 0x08,        // Report Size (8 bits)
    0x15, 0x00,        // Logical Minimum (0)
    0x26, 0xFF, 0x00,  // Logical Maximum (255) — 2-byte encoding keeps value positive (signed)
    0x05, 0x07,        // Usage Page (Key Codes)
    0x19, 0x00,        // Usage Minimum (0)
    0x29, 0xFF,        // Usage Maximum (255)
    0x81, 0x00,        // Input (Data, Array, Absolute)
    0xC0,              // End Collection
};
// clang-format on

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
USBHIDKeyboard *global_usb_hid_keyboard;

// Mirrors the USB configuration state before the component instance exists so
// that setup() can sync ep_ready_ on startup if USB_DC_CONFIGURED already fired.
static std::atomic<bool> s_usb_configured{false};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// Called when USB line state changes (connected, configured, suspended, …).
// Drives ep_ready_ so that send_report_() can gate on actual USB readiness
// rather than inferring it from interrupt-endpoint transfer completions alone.
// int_in_ready_cb still handles flow control between back-to-back reports.
static void usb_status_cb_(enum usb_dc_status_code status, const uint8_t *param) {
  bool ready;
  switch (status) {
    case USB_DC_CONFIGURED:
    case USB_DC_RESUME:
      ready = true;
      break;
    case USB_DC_DISCONNECTED:
    case USB_DC_SUSPEND:
    case USB_DC_ERROR:
      ready = false;
      break;
    default:
      return;
  }
  s_usb_configured.store(ready, std::memory_order_release);
  if (global_usb_hid_keyboard != nullptr) {
    global_usb_hid_keyboard->set_ep_ready(ready);
  }
}

// Called from USB interrupt context when the interrupt-IN endpoint finishes a
// transfer and is ready to accept the next report (flow control between reports).
void USBHIDKeyboard::int_in_ready_cb(const struct device *dev) {
  if (global_usb_hid_keyboard != nullptr) {
    global_usb_hid_keyboard->set_ep_ready(true);
  }
}

static const struct hid_ops kbd_ops = {
    .int_in_ready = USBHIDKeyboard::int_in_ready_cb,
};

// Registers the HID report descriptor and enables USB before main() starts.
//
// The logger's pre_setup() calls usb_enable() to start CDC ACM.  If we wait
// until our setup() to call usb_hid_register_device(), the USB host may
// enumerate and request the HID report descriptor before we have set it,
// receiving a zero-length response and failing with EINVAL.  Running at
// APPLICATION level (priority 40) happens before main() and before any
// ESPHome component pre_setup() or setup(), so the descriptor is always in
// place when the host first connects.
static int usb_hid_keyboard_sys_init_(void) {
  const struct device *dev = device_get_binding("HID_0");
  if (dev == nullptr) {
    return -ENODEV;
  }
  usb_hid_register_device(dev, KBD_REPORT_DESC, sizeof(KBD_REPORT_DESC), &kbd_ops);
  usb_hid_init(dev);
  // Enable USB with a status callback so USB_DC_CONFIGURED sets ep_ready_.
  // If usb_enable() is called again later (e.g. by the CDC ACM logger) it
  // returns -EALREADY without replacing the callback, which is harmless.
  usb_enable(usb_status_cb_);
  return 0;
}
SYS_INIT(usb_hid_keyboard_sys_init_, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

void USBHIDKeyboard::setup() {
  global_usb_hid_keyboard = this;

  // usb_hid_register_device() and usb_enable() were already called in
  // usb_hid_keyboard_sys_init_() to avoid a race with the logger.
  this->hid_dev_ = device_get_binding("HID_0");
  if (this->hid_dev_ == nullptr) {
    ESP_LOGE(TAG, "HID device not found - check CONFIG_USB_DEVICE_HID");
    this->mark_failed();
    return;
  }

  // USB_DC_CONFIGURED may have fired before this component was created, so
  // sync ep_ready_ from the module-level flag set by usb_status_cb_().
  this->set_ep_ready(s_usb_configured.load(std::memory_order_acquire));

  ESP_LOGD(TAG, "USB HID keyboard ready");
}

void USBHIDKeyboard::dump_config() {
  ESP_LOGCONFIG(TAG, "USB HID Keyboard");
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Failed to initialize");
  }
}

void USBHIDKeyboard::send_report_(const KeyboardReport &report) {
  if (this->hid_dev_ == nullptr) {
    return;
  }
  if (!this->ep_ready_.load(std::memory_order_acquire)) {
    ESP_LOGD(TAG, "USB not connected, dropping report");
    return;
  }
  this->ep_ready_.store(false, std::memory_order_release);

  // hid_int_ep_write copies the buffer before returning, so stack storage is safe.
  int ret = hid_int_ep_write(this->hid_dev_, reinterpret_cast<const uint8_t *>(&report), sizeof(report), nullptr);
  if (ret != 0) {
    ESP_LOGW(TAG, "hid_int_ep_write failed (%d)", ret);
    this->ep_ready_.store(true, std::memory_order_release);
  }
}

void USBHIDKeyboard::press_key(uint8_t modifier, uint8_t keycode) {
  KeyboardReport report{};
  report.modifier = modifier;
  report.keys[0] = keycode;
  this->send_report_(report);
}

void USBHIDKeyboard::release_all() { this->send_report_(KeyboardReport{}); }

}  // namespace esphome::usb_hid_keyboard
#endif  // USE_ZEPHYR

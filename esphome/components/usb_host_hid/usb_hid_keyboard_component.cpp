// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP_IDF) && \
    (defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4))
#include "esphome/core/log.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"

#include "usb_hid.h"

namespace esphome {
namespace usb_host_hid {

namespace {

inline bool key_found(const uint8_t *const src, uint8_t key, unsigned int length) {
  for (unsigned int i = 0; i < length; i++) {
    if (src[i] == key)
      return true;
  }
  return false;
}

const uint8_t KEY_CODES_ASCII[57][2] = {
    {0, 0},       /* HID_KEY_NO_PRESS        */
    {0, 0},       /* HID_KEY_ROLLOVER        */
    {0, 0},       /* HID_KEY_POST_FAIL       */
    {0, 0},       /* HID_KEY_ERROR_UNDEFINED */
    {'a', 'A'},   /* HID_KEY_A               */
    {'b', 'B'},   /* HID_KEY_B               */
    {'c', 'C'},   /* HID_KEY_C               */
    {'d', 'D'},   /* HID_KEY_D               */
    {'e', 'E'},   /* HID_KEY_E               */
    {'f', 'F'},   /* HID_KEY_F               */
    {'g', 'G'},   /* HID_KEY_G               */
    {'h', 'H'},   /* HID_KEY_H               */
    {'i', 'I'},   /* HID_KEY_I               */
    {'j', 'J'},   /* HID_KEY_J               */
    {'k', 'K'},   /* HID_KEY_K               */
    {'l', 'L'},   /* HID_KEY_L               */
    {'m', 'M'},   /* HID_KEY_M               */
    {'n', 'N'},   /* HID_KEY_N               */
    {'o', 'O'},   /* HID_KEY_O               */
    {'p', 'P'},   /* HID_KEY_P               */
    {'q', 'Q'},   /* HID_KEY_Q               */
    {'r', 'R'},   /* HID_KEY_R               */
    {'s', 'S'},   /* HID_KEY_S               */
    {'t', 'T'},   /* HID_KEY_T               */
    {'u', 'U'},   /* HID_KEY_U               */
    {'v', 'V'},   /* HID_KEY_V               */
    {'w', 'W'},   /* HID_KEY_W               */
    {'x', 'X'},   /* HID_KEY_X               */
    {'y', 'Y'},   /* HID_KEY_Y               */
    {'z', 'Z'},   /* HID_KEY_Z               */
    {'1', '!'},   /* HID_KEY_1               */
    {'2', '@'},   /* HID_KEY_2               */
    {'3', '#'},   /* HID_KEY_3               */
    {'4', '$'},   /* HID_KEY_4               */
    {'5', '%'},   /* HID_KEY_5               */
    {'6', '^'},   /* HID_KEY_6               */
    {'7', '&'},   /* HID_KEY_7               */
    {'8', '*'},   /* HID_KEY_8               */
    {'9', '('},   /* HID_KEY_9               */
    {'0', ')'},   /* HID_KEY_0               */
    {'\r', '\r'}, /* HID_KEY_ENTER           */
    {0, 0},       /* HID_KEY_ESC             */
    {'\b', 0},    /* HID_KEY_DEL             */
    {0, 0},       /* HID_KEY_TAB             */
    {' ', ' '},   /* HID_KEY_SPACE           */
    {'-', '_'},   /* HID_KEY_MINUS           */
    {'=', '+'},   /* HID_KEY_EQUAL           */
    {'[', '{'},   /* HID_KEY_OPEN_BRACKET    */
    {']', '}'},   /* HID_KEY_CLOSE_BRACKET   */
    {'\\', '|'},  /* HID_KEY_BACK_SLASH      */
    {'\\', '|'},
    /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
    {';', ':'},                    /* HID_KEY_COLON           */
    {'\'', '"'},                   /* HID_KEY_QUOTE           */
    {'`', '~'},                    /* HID_KEY_TILDE           */
    {',', '<'},                    /* HID_KEY_LESS            */
    {'.', '>'},                    /* HID_KEY_GREATER         */
    {'/', '?'}                     /* HID_KEY_SLASH           */
};

inline bool hid_keyboard_is_modifier_shift(uint8_t modifier) {
  return ((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) || ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT);
}

inline bool hid_keyboard_get_char(uint8_t modifier, uint8_t key_code, unsigned char *key_char) {
  uint8_t mod = hid_keyboard_is_modifier_shift(modifier) ? 1 : 0;
  if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
    *key_char = KEY_CODES_ASCII[key_code][mod];
    return true;
  }
  return false;
}

void hid_host_interface_callback(hid_host_device_handle_t handle, const hid_host_interface_event_t event, void *ptr) {
  auto *kbd = static_cast<USBHidKeyboard *>(ptr);
  kbd->on_interface_event(handle, event);
}

}  // namespace

void USBHidKeyboard::dump_config() {
  ESP_LOGCONFIG(TAG, "Keyboard:\n  VID: %04X\n  PID: %04X", this->dev_info_.VID, this->dev_info_.PID);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Connected", this);
#endif
}

void USBHidKeyboard::setup() {}

void USBHidKeyboard::loop() { this->disable_loop(); }

void USBHidKeyboard::open_and_start(hid_host_device_handle_t handle, hid_host_dev_info_t dev_info,
                                    hid_host_dev_params_t dev_params) {
  if (this->state_ != HidClientState::HID_DEVICE_INIT) {
    ESP_LOGW(TAG, "Device %04X:%04X is already started; ignored", dev_info.VID, dev_info.PID);
    return;
  }
  this->handle_ = handle;
  this->dev_info_ = dev_info;
  this->dev_params_ = dev_params;
  const hid_host_device_config_t dev_config = {
      .callback = hid_host_interface_callback,
      .callback_arg = this,
  };
  ESP_ERROR_CHECK(hid_host_device_open(handle, &dev_config));
  if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
    ESP_ERROR_CHECK(hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT));
    if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
      ESP_ERROR_CHECK(hid_class_request_set_idle(handle, 0, 0));
    }
  }
  ESP_ERROR_CHECK(hid_host_device_start(handle));
  ESP_LOGD(TAG, "Device %04X:%04X is listening for keys", dev_info.VID, dev_info.PID);
  this->state_ = HidClientState::HID_DEVICE_CONNECTED;
#ifdef USE_BINARY_SENSOR
  this->publish_state(true);
#endif
}

void USBHidKeyboard::on_interface_event(hid_host_device_handle_t handle, hid_host_interface_event_t event) {
  if (handle != this->handle_)
    return;
  switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
      this->on_key_pressed();
      break;
    }
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "Device %04X:%04X disconnected", this->dev_info_.VID, this->dev_info_.PID);
      hid_host_device_close(this->handle_);
      this->state_ = HidClientState::HID_DEVICE_INIT;
#ifdef USE_BINARY_SENSOR
      this->publish_state(false);
#endif
      break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
      ESP_LOGD(TAG, "Device %04X:%04X had a TRANSFER_ERROR; ignored", this->dev_info_.VID, this->dev_info_.PID);
      break;
    default:
      break;
  }
}

void USBHidKeyboard::on_key_pressed() {
  static constexpr size_t SIZE = 64;
  uint8_t data[SIZE] = {0};
  size_t length = 0;
  ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(this->handle_, data, SIZE, &length));
  if (length < sizeof(hid_keyboard_input_report_boot_t))
    return;
  auto *kb_report = (hid_keyboard_input_report_boot_t *) data;
  unsigned char key;
  for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
    if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
        !key_found(this->prev_keys_, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
      const auto key_code = kb_report->key[i];
      const auto modifier = kb_report->modifier.val;
      if (hid_keyboard_get_char(modifier, key_code, &key)) {
        if (key > 0) {
          ESP_LOGV(TAG, "Device %04X:%04X pressed key: %d, '%c'", this->dev_info_.VID, this->dev_info_.PID, key, key);
          this->send_key_(key);
        }
      }
    }
  }
  memcpy(this->prev_keys_, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

}  // namespace usb_host_hid
}  // namespace esphome

#endif

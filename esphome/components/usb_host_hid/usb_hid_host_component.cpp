// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP_IDF) && \
    (defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4))
#include "esphome/core/log.h"
#include "usb/hid_host.h"
#include "usb/usb_host.h"

#include <cwchar>

#include "usb_hid.h"

namespace esphome {
namespace usb_host_hid {

namespace {

void hid_host_device_callback(hid_host_device_handle_t handle, const hid_host_driver_event_t event, void *ptr) {
  auto *host = static_cast<USBHidHost *>(ptr);
  host->on_event(handle, event);
}

}  // namespace

void USBHidHost::dump_config() { ESP_LOGCONFIG(TAG, "USB HID host"); }

void USBHidHost::setup() {
  usb_host_config_t config{};

  if (usb_host_install(&config) != ESP_OK) {
    this->status_set_error("usb_host_install failed");
    this->mark_failed();
    return;
  }

  const hid_host_driver_config_t hid_host_driver_config = {
      .create_background_task = false,
      .task_priority = 5,
      .stack_size = 4096,
      .core_id = 0,
      .callback = hid_host_device_callback,
      .callback_arg = this,
  };
  if (hid_host_install(&hid_host_driver_config) != ESP_OK) {
    ESP_LOGW(TAG, "hid_host_install err");
    this->status_set_error("hid_host_install failed");
    this->mark_failed();
    return;
  }
}

void USBHidHost::loop() {
  int err;
  uint32_t event_flags;
  err = usb_host_lib_handle_events(0, &event_flags);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGD(TAG, "lib_handle_events failed: %s", esp_err_to_name(err));
  }
  if (event_flags != 0) {
    ESP_LOGD(TAG, "Event flags %" PRIu32 "X", event_flags);
  }

  err = hid_host_handle_events(0);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGD(TAG, "hid_host_handle_events failed: %s", esp_err_to_name(err));
  }
}

void USBHidHost::register_keyboard(USBHidKeyboard *kbd, bool check_class) {
  this->keyboards_.emplace_back(kbd, check_class);
}

#define WCHAR_CONV(s) \
  ({ \
    char __buf[HID_STR_DESC_MAX_LENGTH]; \
    __buf[HID_STR_DESC_MAX_LENGTH - 1] = '\0'; \
    wcstombs(__buf, (s), HID_STR_DESC_MAX_LENGTH); \
    __buf; \
  })

void USBHidHost::on_event(hid_host_device_handle_t handle, const hid_host_driver_event_t event) {
  hid_host_dev_info_t dev_info;
  ESP_ERROR_CHECK(hid_host_get_device_info(handle, &dev_info));
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(handle, &dev_params));
  switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
      ESP_LOGD(TAG, "Device connected: %04X:%04X, Manufacturer: '%s', Product: '%s', Serial Number: '%s'", dev_info.VID,
               dev_info.PID, WCHAR_CONV(dev_info.iManufacturer), WCHAR_CONV(dev_info.iProduct),
               WCHAR_CONV(dev_info.iSerialNumber));
      auto it = std::find_if(this->keyboards_.begin(), this->keyboards_.end(),
                             [&dev_info](const KeyboardConfig &conf) { return conf.first->matches_(dev_info); });
      if (it == this->keyboards_.end()) {
        ESP_LOGD(TAG, "Unregistered device %04X:%04X connected; ignored", dev_info.VID, dev_info.PID);
        return;
      }
      auto [keyboard, check_class] = *it;
      if (check_class) {
        if (dev_params.sub_class != HID_SUBCLASS_BOOT_INTERFACE) {
          ESP_LOGW(TAG, "Registered device %04X:%04X has unsupported HID sub-class: %d; want: %d (BOOT_INTERFACE)",
                   dev_info.VID, dev_info.PID, dev_params.sub_class, HID_SUBCLASS_BOOT_INTERFACE);
          return;
        }
        if (dev_params.proto != HID_PROTOCOL_KEYBOARD) {
          ESP_LOGW(TAG, "Registered device %04X:%04X has unsupported HID protocol: %d; want: %d (KEYBOARD)",
                   dev_info.VID, dev_info.PID, dev_params.proto, HID_PROTOCOL_KEYBOARD);
          return;
        }
      }
      ESP_LOGI(TAG, "Registered device %04X:%04X connected", dev_info.VID, dev_info.PID);
      keyboard->open_and_start(handle, std::move(dev_info), std::move(dev_params));
      break;
  }
}

#undef WCHAR_CONV

}  // namespace usb_host_hid
}  // namespace esphome

#endif

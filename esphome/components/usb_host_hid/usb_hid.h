#pragma once

// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP_IDF) && \
    (defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4))
#include "esphome/core/component.h"
#include "esphome/components/key_provider/key_provider.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"

#include <vector>
#include <cinttypes>

namespace esphome {
namespace usb_host_hid {

static const char *const TAG = "usb_hid";

class USBHidClient;

enum class HidClientState {
  HID_DEVICE_INIT = 0,
  HID_DEVICE_CONNECTED,
};

class USBHidKeyboard : public Component,
#ifdef USE_BINARY_SENSOR
                       public binary_sensor::BinarySensorInitiallyOff,
#endif
                       public key_provider::KeyProvider {
  friend class USBHidHost;

 public:
  USBHidKeyboard(uint16_t vid, uint16_t pid) : dev_info_{.VID = vid, .PID = pid} {}

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::LATE; }
  void dump_config() override;

  void open_and_start(hid_host_device_handle_t handle, hid_host_dev_info_t dev_info, hid_host_dev_params_t dev_params);
  void on_interface_event(hid_host_device_handle_t hid_device_handle, hid_host_interface_event_t event);
  void on_key_pressed();

 protected:
  hid_host_device_handle_t handle_{};
  hid_host_dev_info_t dev_info_;
  hid_host_dev_params_t dev_params_{};
  HidClientState state_{HidClientState::HID_DEVICE_INIT};
  uint8_t prev_keys_[HID_KEYBOARD_KEY_MAX] = {0};

  inline bool matches_(const hid_host_dev_info_t &dev_info) const {
    const bool matches_vid = this->dev_info_.VID == 0 || dev_info.VID == this->dev_info_.VID;
    const bool matches_pid = this->dev_info_.PID == 0 || dev_info.PID == this->dev_info_.PID;
    return matches_vid && matches_pid;
  }
};

class USBHidHost : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void register_keyboard(USBHidKeyboard *kbd, bool check_class);
  void on_event(hid_host_device_handle_t handle, hid_host_driver_event_t event);

 protected:
  using KeyboardConfig = std::pair<USBHidKeyboard *, bool>;
  std::vector<KeyboardConfig> keyboards_{};
};

}  // namespace usb_host_hid
}  // namespace esphome

#endif

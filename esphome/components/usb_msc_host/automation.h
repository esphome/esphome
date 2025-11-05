#pragma once

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/core/automation.h"
#include "usb_msc_host.h"

namespace esphome {
namespace usb_msc_host {

// Triggers
class DeviceMountedTrigger : public Trigger<std::string> {
 public:
  explicit DeviceMountedTrigger(USBMscDevice *parent) {
    parent->add_mount_ready_callback([this](const std::string &mount_path) { this->trigger(mount_path); });
  }
};

// Actions
template<typename... Ts> class RemountDeviceAction : public Action<Ts...> {
 public:
  explicit RemountDeviceAction(USBMscDevice *device) : device_(device) {}

  void play(Ts... x) override {
    if (this->device_->remount_device()) {
      ESP_LOGI("usb_msc_host", "Device remounted successfully via automation");
    } else {
      ESP_LOGE("usb_msc_host", "Failed to remount device via automation");
    }
  }

 protected:
  USBMscDevice *device_;
};

template<typename... Ts> class UnmountDeviceAction : public Action<Ts...> {
 public:
  explicit UnmountDeviceAction(USBMscDevice *device) : device_(device) {}

  void play(Ts... x) override {
    this->device_->unmount_device();
    ESP_LOGI("usb_msc_host", "Device unmounted via automation");
  }

 protected:
  USBMscDevice *device_;
};

template<typename... Ts> class ListFilesAction : public Action<Ts...> {
 public:
  explicit ListFilesAction(USBMscDevice *device) : device_(device) {}

  void play(Ts... x) override { this->device_->list_files(); }

 protected:
  USBMscDevice *device_;
};

// Conditions
template<typename... Ts> class DeviceMountedCondition : public Condition<Ts...> {
 public:
  explicit DeviceMountedCondition(USBMscDevice *device) : device_(device) {}

  bool check(Ts... x) override { return this->device_->is_mounted(); }

 protected:
  USBMscDevice *device_;
};

}  // namespace usb_msc_host
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4

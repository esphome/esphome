#pragma once

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)

#include "esphome/core/automation.h"
#include "usb_storage.h"

namespace esphome::usb_storage {

// Triggers
class DeviceMountedTrigger : public Trigger<const char *> {
 public:
  explicit DeviceMountedTrigger(USBStorageDevice *parent) {
    parent->add_on_mounted_callback([this](const char *mount_path) { this->trigger(mount_path); });
  }
};

// Actions
template<typename... Ts> class RemountDeviceAction : public Action<Ts...> {
 public:
  explicit RemountDeviceAction(USBStorageDevice *device) : device_(device) {}

  void play(Ts... x) override { this->device_->remount_device(); }

 protected:
  USBStorageDevice *device_;
};

template<typename... Ts> class UnmountDeviceAction : public Action<Ts...> {
 public:
  explicit UnmountDeviceAction(USBStorageDevice *device) : device_(device) {}

  void play(Ts... x) override { this->device_->unmount_device(); }

 protected:
  USBStorageDevice *device_;
};

template<typename... Ts> class ListFilesAction : public Action<Ts...> {
 public:
  explicit ListFilesAction(USBStorageDevice *device) : device_(device) {}

  TEMPLATABLE_VALUE(const char *, path)

  void play(Ts... x) override {
    const char *path = this->path_.value(x...);
    if (path == nullptr || path[0] == '\0')
      path = this->device_->get_mount_path();

    this->device_->log_list_dir_start_(path);
    this->device_->list_dir(path, &USBStorageDevice::log_list_dir_entry, nullptr);
  }

 protected:
  USBStorageDevice *device_;
};

// Conditions
template<typename... Ts> class DeviceMountedCondition : public Condition<Ts...> {
 public:
  explicit DeviceMountedCondition(USBStorageDevice *device) : device_(device) {}

  bool check(Ts... x) override { return this->device_->is_mounted(); }

 protected:
  USBStorageDevice *device_;
};

}  // namespace esphome::usb_storage

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4

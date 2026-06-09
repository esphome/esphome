// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_host.h"
#include <cinttypes>
#include "esphome/core/log.h"

namespace esphome::usb_host {

void USBHost::setup() {
  usb_host_config_t config{};

  if (usb_host_install(&config) != ESP_OK) {
    this->status_set_error(LOG_STR("usb_host_install failed"));
    this->mark_failed();
    return;
  }
}
void USBHost::loop() {
  int err;
  uint32_t event_flags;
  err = usb_host_lib_handle_events(0, &event_flags);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGD(TAG, "lib_handle_events failed failed: %s", esp_err_to_name(err));
  }
  if (event_flags != 0) {
    ESP_LOGD(TAG, "Event flags %" PRIu32 "X", event_flags);
  }
}

void USBClassRouter::on_connected() {
  // Skip if another USBClient already claimed this device address.
  if (this->clients_ != nullptr) {
    for (auto *client : *this->clients_) {
      if (client != this && client->get_device_addr() == this->device_addr_ &&
          client->get_state() == USB_CLIENT_CONNECTED) {
        this->disconnect();
        return;
      }
    }
  }

  const usb_device_desc_t *dev_desc;
  if (usb_host_get_device_descriptor(this->device_handle_, &dev_desc) != ESP_OK) {
    this->disconnect();
    return;
  }

  const usb_config_desc_t *cfg_desc;
  if (usb_host_get_active_config_descriptor(this->device_handle_, &cfg_desc) != ESP_OK) {
    this->disconnect();
    return;
  }

  uint8_t addr = static_cast<uint8_t>(this->device_addr_);
  int offset = 0;
  const usb_standard_desc_t *desc = reinterpret_cast<const usb_standard_desc_t *>(cfg_desc);

  while ((desc = usb_parse_next_descriptor_of_type(desc, cfg_desc->wTotalLength, USB_W_VALUE_DT_INTERFACE,
                                                   &offset)) != nullptr) {
    const auto *intf = reinterpret_cast<const usb_intf_desc_t *>(desc);
    for (auto *driver : this->drivers_) {
      if (driver->claim_interface(intf, dev_desc)) {
        ESP_LOGD(TAG, "Class driver claimed interface %d on device %d", intf->bInterfaceNumber, addr);
        this->claimed_.push_back({addr, this->device_handle_, driver});
        driver->on_interface_claimed(addr, intf->bInterfaceNumber);
        break;
      }
    }
  }

  // Release our probe handle — drivers will open the device via their own client handles.
  this->disconnect();
}

void USBClassRouter::on_removed(usb_device_handle_t handle) {
  for (auto it = this->claimed_.begin(); it != this->claimed_.end();) {
    if (it->handle == handle) {
      it->driver->on_device_disconnected(it->addr);
      it = this->claimed_.erase(it);
    } else {
      ++it;
    }
  }
  USBClient::on_removed(handle);
}

}  // namespace esphome::usb_host

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_host.h"
#include "esphome/core/log.h"

namespace esphome::usb_host {

void USBClassRouter::on_connected() {
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
        this->claimed_.push_back({addr, intf->bInterfaceNumber, driver});
        driver->on_interface_claimed(addr, intf->bInterfaceNumber);
        break;
      }
    }
  }

  // Store the handle before disconnect() clears it, so on_removed() can match it.
  usb_device_handle_t probe_handle = this->device_handle_;
  for (auto &entry : this->claimed_) {
    if (entry.addr == addr)
      entry.handle = probe_handle;
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
  // Also call base to reset state if this handle was still open (e.g. probe failed mid-way).
  USBClient::on_removed(handle);
}

}  // namespace esphome::usb_host

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3

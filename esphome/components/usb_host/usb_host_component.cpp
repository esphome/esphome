// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
#include "usb_host.h"
#include <cinttypes>
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace usb_host {

// NEW: Coordinator event callback for interface-class based device dispatch
// This callback receives device events and triggers interface-class matching
static void coordinator_event_cb(const usb_host_client_event_msg_t *event_msg, void *ptr) {
  auto *host = static_cast<USBHost *>(ptr);

  switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      ESP_LOGV(TAG, "Coordinator: New device %d", event_msg->new_dev.address);
      // Try to dispatch to interface-class handlers (if not claimed by VID/PID)
      host->try_dispatch_to_handlers(event_msg->new_dev.address);
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      ESP_LOGV(TAG, "Coordinator: Device gone");
      // Handler disconnection is managed by the handlers themselves
      break;
    default:
      break;
  }
}

void USBHost::setup() {
  usb_host_config_t config{};

  if (usb_host_install(&config) != ESP_OK) {
    this->status_set_error("usb_host_install failed");
    this->mark_failed();
    return;
  }

  // NEW: Register coordinator client for interface-class handler dispatch
  // This client receives device events and coordinates with VID/PID clients
  usb_host_client_config_t client_config{
      .is_synchronous = false,
      .max_num_event_msg = 5,
      .async = {.client_event_callback = coordinator_event_cb, .callback_arg = this}};

  auto err = usb_host_client_register(&client_config, &this->coordinator_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Coordinator client registration failed: %s", esp_err_to_name(err));
    // Non-fatal: VID/PID clients will still work
  } else {
    ESP_LOGD(TAG, "Coordinator client registered for interface-class handlers");
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

  // NEW: Handle coordinator client events for interface-class handlers
  // Without this, coordinator_event_cb() is never called!
  if (this->coordinator_handle_ != nullptr) {
    usb_host_client_handle_events(this->coordinator_handle_, 0);
  }
}

// NEW: Device claiming system implementation
bool USBHost::try_claim_device(uint8_t address) {
  if (this->claimed_devices_.count(address) > 0) {
    ESP_LOGV(TAG, "Device %d already claimed", address);
    return false;  // Already claimed
  }
  this->claimed_devices_.insert(address);
  ESP_LOGD(TAG, "Device %d claimed by VID/PID client", address);
  return true;
}

void USBHost::release_device(uint8_t address) {
  this->claimed_devices_.erase(address);
  ESP_LOGD(TAG, "Device %d released", address);
}

void USBHost::register_device_handler(USBDeviceHandler *handler) {
  this->handlers_.push_back(handler);
  ESP_LOGD(TAG, "Registered interface-class device handler");
}

void USBHost::try_dispatch_to_handlers(uint8_t address) {
  // Small delay to allow VID/PID clients to claim first
  // VID/PID clients receive events simultaneously and may claim during their loop()
  vTaskDelay(pdMS_TO_TICKS(10));

  // Check if already claimed by VID/PID client
  if (this->claimed_devices_.count(address) > 0) {
    ESP_LOGV(TAG, "Device %d already claimed by VID/PID client, skipping interface-class handlers", address);
    return;
  }

  // Device not claimed - try interface-class handlers
  usb_device_handle_t device_handle;
  esp_err_t err = usb_host_device_open(this->coordinator_handle_, address, &device_handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to open device %d for handler dispatch: %s", address, esp_err_to_name(err));
    return;
  }

  // Get config descriptor
  const usb_config_desc_t *config_desc;
  err = usb_host_get_active_config_descriptor(device_handle, &config_desc);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to get config descriptor for device %d: %s", address, esp_err_to_name(err));
    usb_host_device_close(this->coordinator_handle_, device_handle);
    return;
  }

  // Try all registered interface-class handlers
  for (auto *handler : this->handlers_) {
    if (handler->matches_device(config_desc)) {
      ESP_LOGI(TAG, "Device %d matched by interface-class handler", address);
      this->claimed_devices_.insert(address);  // Mark as claimed
      handler->on_device_connected(device_handle, address);
      return;  // Handler takes ownership of device_handle
    }
  }

  // No handler matched
  ESP_LOGV(TAG, "No interface-class handler matched device %d", address);
  usb_host_device_close(this->coordinator_handle_, device_handle);
}

void USBHost::close_device_handle(usb_device_handle_t device_handle) {
  if (device_handle != nullptr) {
    usb_host_device_close(this->coordinator_handle_, device_handle);
  }
}

void USBHost::add_device_to_whitelist(uint16_t vid, uint16_t pid) {
  this->device_whitelist_.push_back(std::make_pair(vid, pid));
  ESP_LOGD(TAG, "Added device to whitelist: VID=0x%04X, PID=0x%04X", vid, pid);
}

bool USBHost::is_device_whitelisted(uint16_t vid, uint16_t pid) const {
  // Empty whitelist means all devices allowed
  if (this->device_whitelist_.empty()) {
    return true;
  }

  // Check if device is in whitelist
  for (const auto &device : this->device_whitelist_) {
    if (device.first == vid && device.second == pid) {
      return true;
    }
    // Wildcard (0, 0) matches all devices
    if (device.first == 0 && device.second == 0) {
      return true;
    }
  }

  return false;
}

}  // namespace usb_host
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3

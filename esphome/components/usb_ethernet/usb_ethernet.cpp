#include "esphome/components/usb_ethernet/usb_ethernet.h"
#include "esphome/core/application.h"

extern "C" {
  #include "cdc_ecm_host.h"
  #include "cdc_host_descriptor_parsing.h"
  #include "cdc_host_types.h"
  #include "usb_types_cdc.h"
  #include "esp_event.h"
  #include "esp_log.h"
  #include "esp_netif.h"
  #include "esp_eth.h"
  #include "esp_heap_caps.h"
  #include "esp_netif_net_stack.h"
  #include "lwip/igmp.h"
  #include "lwip/ip4_addr.h"
  #include "lwip/netif.h"
}

namespace esphome {
namespace usb_ethernet {

const char *const TAG = "usb_ethernet";

USBEthernetComponent *global_usb_eth_component = nullptr;

// Realtek 8152/8153 Vendor and Product IDs
#define USB_DEVICE_VID   0x0BDA
#define USB_DEVICE_PID_1 0x8152
#define USB_DEVICE_PID_2 0x8153

// Forward declaration for the event handler used by esp_event and the CDC driver
static void netif_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data);

// External reference to the global usb_netif from cdc_ecm_host.c
extern "C" {
  extern esp_netif_t *usb_netif;
}

void USBEthernetComponent::set_has_ip(bool has_ip) {
  this->has_ip_ = has_ip;
  if (!has_ip) {
    this->connected_ = false;
    this->ip_addresses_.fill(network::IPAddress{});
  }
}

void USBEthernetComponent::set_primary_ip(const network::IPAddress &ip) {
  this->ip_addresses_.fill(network::IPAddress{});
  this->ip_addresses_[0] = ip;
  this->has_ip_ = true;
  this->connected_ = true;  // set this true when we have a v4 addr
}

bool USBEthernetComponent::is_connected() {
  return this->connected_;
}

network::IPAddresses USBEthernetComponent::get_ip_addresses() const {
  return this->ip_addresses_;
}

const char *USBEthernetComponent::get_use_address() const {
  // If user or setup() already set a hostname/address, return that
  if (!this->use_address_.empty())
    return this->use_address_.c_str();

  // Fallback: "<node_name>.local"
  return "";
}

void USBEthernetComponent::set_use_address(const char *use_address) {
  if (use_address == nullptr) {
    this->use_address_.clear();
  } else {
    this->use_address_ = use_address;
  }
}

void USBEthernetComponent::setup() {
  ESP_LOGI(TAG, "Initializing USB CDC-ECM Host (Realtek 8152/8153)");

  ESP_LOGI(TAG, "Total Heap: %d", heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
  ESP_LOGI(TAG, "Free Heap: %d", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
  ESP_LOGI(TAG, "PSRAM Size: %d", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));

  if (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) == 0) {
    ESP_LOGE(TAG, "CRITICAL: PSRAM NOT DETECTED! USB Host will fail.");
  }

  // Expose this instance globally so network::util can see us
  global_usb_eth_component = this;

  // Initialize our hostname / use_address_ to "<node_name>.local"
  this->use_address_.clear();
  this->use_address_ = App.get_name();
  this->use_address_ += ".local";

  // Ensure the default event loop is running
  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(err));
    return;
  }

  // Ensure esp_netif is initialized
  err = esp_netif_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to initialize esp_netif: %s", esp_err_to_name(err));
    return;
  }

  // Optional: turn up logging
  esp_log_level_set("cdc_ecm", ESP_LOG_DEBUG);
  esp_log_level_set("USB-CDC", ESP_LOG_DEBUG);

  // Register event handlers
  ESP_ERROR_CHECK(esp_event_handler_register(
      ETH_EVENT, ESP_EVENT_ANY_ID, &netif_event_handler, this));
  ESP_ERROR_CHECK(esp_event_handler_register(
      IP_EVENT, IP_EVENT_ETH_GOT_IP, &netif_event_handler, this));

  static cdc_ecm_params_t cdc_ecm_params = {
      .vid = USB_DEVICE_VID,
      .pids = {USB_DEVICE_PID_1, USB_DEVICE_PID_2},
      .event_cb = netif_event_handler,
      .callback_arg = nullptr,  // we rely on esp_event handlers for 'self'
      .hostname = (char *)"esphome-ethernet",
  };

  cdc_ecm_init(&cdc_ecm_params);
  ESP_LOGI(TAG, "USB CDC-ECM Host task started.");
}

void USBEthernetComponent::loop() {

}

void USBEthernetComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "USB Ethernet (CDC-ECM / Realtek 8152/8153):");
  if (this->has_ip_) {
    // Slot 0 is the primary IP
    ESP_LOGCONFIG(TAG, "  IP Address: %s", this->ip_addresses_[0].str().c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  IP Address: (not assigned)");
  }
}

static void netif_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
  auto *self = static_cast<USBEthernetComponent *>(arg);

  if (event_base == ETH_EVENT) {
    switch (event_id) {
      case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Connected, waiting for IP...");
        if (self != nullptr) {
          self->set_link_up(true);
          self->set_has_ip(false);
        }
        break;
      case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Disconnected");
        if (self != nullptr) {
          self->set_link_up(false);
          self->set_has_ip(false);
        }
        break;
      case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        if (self != nullptr) {
          self->set_link_up(false);
          self->set_has_ip(false);
        }
        break;
      default:
        break;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
    auto *event = (ip_event_got_ip_t *) event_data;

    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

    if (self != nullptr) {
      uint32_t addr = event->ip_info.ip.addr;
      uint8_t b1 = (addr >> 0) & 0xFF;
      uint8_t b2 = (addr >> 8) & 0xFF;
      uint8_t b3 = (addr >> 16) & 0xFF;
      uint8_t b4 = (addr >> 24) & 0xFF;
      network::IPAddress ipaddr(b1, b2, b3, b4);
      self->set_primary_ip(ipaddr);
      

    }
  }
}

}  // namespace usb_ethernet
}  // namespace esphome

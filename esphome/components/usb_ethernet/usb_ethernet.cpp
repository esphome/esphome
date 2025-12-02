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
  if (this->has_ip_ != has_ip) {
    ESP_LOGD(TAG, "has_ip changed: %d -> %d", this->has_ip_, has_ip);
  }
  this->has_ip_ = has_ip;
  if (!has_ip) {
    if (this->connected_) {
      ESP_LOGW(TAG, "Clearing connected state (was connected, now has_ip=false)");
    }
    this->connected_ = false;
    this->ip_addresses_.fill(network::IPAddress{});
  }
}

void USBEthernetComponent::set_primary_ip(const network::IPAddress &ip) {
  this->ip_addresses_.fill(network::IPAddress{});
  this->ip_addresses_[0] = ip;
  this->has_ip_ = true;
  if (!this->connected_) {
    ESP_LOGI(TAG, "Setting connected state to true (got IP: %s)", ip.str().c_str());
  }
  this->connected_ = true;  // set this true when we have a v4 addr
}

void USBEthernetComponent::apply_manual_ip() {
  if (!this->manual_ip_.has_value()) {
    return;
  }
  
  if (usb_netif == nullptr) {
    ESP_LOGE(TAG, "Cannot configure manual IP: netif not available");
    return;
  }

  ESP_LOGI(TAG, "Configuring Manual IP...");
  
  // Stop DHCP client
  esp_err_t err = esp_netif_dhcpc_stop(usb_netif);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    ESP_LOGE(TAG, "Failed to stop DHCP client: %s", esp_err_to_name(err));
  }

  // Set static IP info
  esp_netif_ip_info_t ip_info;
  ip_info.ip = this->manual_ip_->static_ip;
  ip_info.gw = this->manual_ip_->gateway;
  ip_info.netmask = this->manual_ip_->subnet;

  err = esp_netif_set_ip_info(usb_netif, &ip_info);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set manual IP: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "Manual IP configured: %s", this->manual_ip_->static_ip.str().c_str());
  }

  // Set DNS servers if configured
  if (this->manual_ip_->dns1 != network::IPAddress(0, 0, 0, 0)) {
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4 = this->manual_ip_->dns1;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(usb_netif, ESP_NETIF_DNS_MAIN, &dns);
  }

  if (this->manual_ip_->dns2 != network::IPAddress(0, 0, 0, 0)) {
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4 = this->manual_ip_->dns2;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(usb_netif, ESP_NETIF_DNS_BACKUP, &dns);
  }
  
  // Set MAC address if configured
  if (this->fixed_mac_.has_value()) {
    err = esp_netif_set_mac(usb_netif, this->fixed_mac_->data());
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set MAC address: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
               (*this->fixed_mac_)[0], (*this->fixed_mac_)[1], (*this->fixed_mac_)[2],
               (*this->fixed_mac_)[3], (*this->fixed_mac_)[4], (*this->fixed_mac_)[5]);
    }
  }
  
  // Note: No need to manually post IP_EVENT_ETH_GOT_IP
  // The netif has ESP_NETIF_FLAG_EVENT_IP_MODIFIED set, so esp_netif_set_ip_info()
  // automatically posts the event, just like the standard ethernet component
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

  static cdc_ecm_params_t cdc_ecm_params = {
      .vid = USB_DEVICE_VID,
      .pids = {USB_DEVICE_PID_1, USB_DEVICE_PID_2},
      .event_cb = netif_event_handler,
      .callback_arg = this,  // Pass 'this' so the event handler can access the component
      .hostname = (char *)"esphome-ethernet",
      .nameserver = nullptr,
      .if_key = nullptr,
      .if_desc = nullptr,
      .disable_dhcp = this->manual_ip_.has_value(),  // Disable DHCP if manual IP is configured
  };

  cdc_ecm_init(&cdc_ecm_params);
  ESP_LOGI(TAG, "USB CDC-ECM Host task started.");
}

void USBEthernetComponent::loop() {
  // For manual IP, periodically re-post IP event to keep mDNS alive
  // DHCP does this naturally, but with manual IP we need to do it explicitly
  if (this->manual_ip_.has_value() && this->connected_ && usb_netif != nullptr) {
    static uint32_t last_refresh = 0;
    uint32_t now = millis();
    
    // Re-post IP event every 60 seconds
    if (now - last_refresh > 60000 || last_refresh == 0) {
      last_refresh = now;
      
      // Verify we still have the IP configured
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(usb_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        // Re-post IP event to keep mDNS and other services alive
        ip_event_got_ip_t event;
        event.esp_netif = usb_netif;
        event.ip_info = ip_info;
        event.ip_changed = false;
        esp_event_post(IP_EVENT, IP_EVENT_ETH_GOT_IP, &event, sizeof(event), 0);
        ESP_LOGV(TAG, "Refreshed IP event for mDNS (manual IP mode)");
      }
    }
  }
}

float USBEthernetComponent::get_setup_priority() const { return setup_priority::WIFI; }

void USBEthernetComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "USB Ethernet (CDC-ECM / Realtek 8152/8153):");
  
  // Display use_address
  ESP_LOGCONFIG(TAG, "  Use Address: %s", this->use_address_.c_str());
  
  // Display MAC address if set
  if (this->fixed_mac_.has_value()) {
    ESP_LOGCONFIG(TAG, "  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
                 (*this->fixed_mac_)[0], (*this->fixed_mac_)[1], (*this->fixed_mac_)[2],
                 (*this->fixed_mac_)[3], (*this->fixed_mac_)[4], (*this->fixed_mac_)[5]);
  }
  
  // Display manual IP if configured
  if (this->manual_ip_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Manual IP:");
    ESP_LOGCONFIG(TAG, "    Static IP: %s", this->manual_ip_->static_ip.str().c_str());
    ESP_LOGCONFIG(TAG, "    Gateway: %s", this->manual_ip_->gateway.str().c_str());
    ESP_LOGCONFIG(TAG, "    Subnet: %s", this->manual_ip_->subnet.str().c_str());
    if (this->manual_ip_->dns1 != network::IPAddress(0, 0, 0, 0)) {
      ESP_LOGCONFIG(TAG, "    DNS1: %s", this->manual_ip_->dns1.str().c_str());
    }
    if (this->manual_ip_->dns2 != network::IPAddress(0, 0, 0, 0)) {
      ESP_LOGCONFIG(TAG, "    DNS2: %s", this->manual_ip_->dns2.str().c_str());
    }
  }
  
  // Display current IP
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
          // Apply manual IP configuration if set, otherwise wait for DHCP
          if (self->has_manual_ip()) {
            self->apply_manual_ip();
          } else {
            self->set_has_ip(false);
          }
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
          if (!self->has_manual_ip()) {
            self->set_has_ip(false);
          }
        }
        break;
      default:
        break;
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
    auto *event = (ip_event_got_ip_t *) event_data;

    if (self != nullptr) {
      uint32_t addr = event->ip_info.ip.addr;
      uint8_t b1 = (addr >> 0) & 0xFF;
      uint8_t b2 = (addr >> 8) & 0xFF;
      uint8_t b3 = (addr >> 16) & 0xFF;
      uint8_t b4 = (addr >> 24) & 0xFF;
      network::IPAddress ipaddr(b1, b2, b3, b4);
      
      // Only log if IP actually changed or if not using manual IP
      auto current_ips = self->get_ip_addresses();
      if (!self->has_manual_ip() || current_ips[0] != ipaddr) {
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
      } else {
        ESP_LOGV(TAG, "IP event (refresh): " IPSTR, IP2STR(&event->ip_info.ip));
      }
      
      self->set_primary_ip(ipaddr);
    }
  }
}

}  // namespace usb_ethernet
}  // namespace esphome

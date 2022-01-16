#include "ethernet_component.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <eth_phy/phy_lan8720.h>
#include <eth_phy/phy_tlk110.h>
#include <lwip/dns.h>

/// Macro for IDF version comparison
#ifndef ESP_IDF_VERSION_VAL
#define ESP_IDF_VERSION_VAL(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#endif

// Defined in WiFiGeneric.cpp, sets global initialized flag, starts network event task queue and calls
// tcpip_adapter_init()
extern void tcpipInit();  // NOLINT(readability-identifier-naming)

namespace esphome {
namespace ethernet {

static const char *const TAG = "ethernet";

EthernetComponent *global_eth_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#define ESPHL_ERROR_CHECK(err, message) \
  if ((err) != ESP_OK) { \
    ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
    this->mark_failed(); \
    return; \
  }

EthernetComponent::EthernetComponent() { global_eth_component = this; }
void EthernetComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Ethernet...");

  auto f = std::bind(&EthernetComponent::on_wifi_event_, this, std::placeholders::_1, std::placeholders::_2);
  WiFi.onEvent(f);

  if (this->power_pin_ != nullptr) {
    this->power_pin_->setup();
  }

  switch (this->type_) {
    case ETHERNET_TYPE_LAN8720: {
      memcpy(&this->eth_config_, &phy_lan8720_default_ethernet_config, sizeof(eth_config_t));
      break;
    }
    case ETHERNET_TYPE_TLK110: {
      memcpy(&this->eth_config_, &phy_tlk110_default_ethernet_config, sizeof(eth_config_t));
      break;
    }
    default: {
      this->mark_failed();
      return;
    }
  }

  this->eth_config_.phy_addr = static_cast<eth_phy_base_t>(this->phy_addr_);
  this->eth_config_.clock_mode = this->clk_mode_;
  this->eth_config_.gpio_config = EthernetComponent::eth_phy_config_gpio;
  this->eth_config_.tcpip_input = tcpip_adapter_eth_input;

  if (this->power_pin_ != nullptr) {
    this->orig_power_enable_fun_ = this->eth_config_.phy_power_enable;
    this->eth_config_.phy_power_enable = EthernetComponent::eth_phy_power_enable;
  }

  tcpipInit();

  esp_err_t err;
  err = esp_eth_init(&this->eth_config_);
  ESPHL_ERROR_CHECK(err, "ETH init error");
  err = esp_eth_enable();
  ESPHL_ERROR_CHECK(err, "ETH enable error");
}
void EthernetComponent::loop() {
  const uint32_t now = millis();

  switch (this->state_) {
    case EthernetComponentState::STOPPED:
      if (this->started_) {
        ESP_LOGI(TAG, "Starting ethernet connection");
        this->state_ = EthernetComponentState::CONNECTING;
        this->start_connect_();
      }
      break;
    case EthernetComponentState::CONNECTING:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped ethernet connection");
        this->state_ = EthernetComponentState::STOPPED;
      } else if (this->connected_) {
        // connection established
        ESP_LOGI(TAG, "Connected via Ethernet!");
        this->state_ = EthernetComponentState::CONNECTED;

        this->dump_connect_params_();
        this->status_clear_warning();
      } else if (now - this->connect_begin_ > 15000) {
        ESP_LOGW(TAG, "Connecting via ethernet failed! Re-connecting...");
        this->start_connect_();
      }
      break;
    case EthernetComponentState::CONNECTED:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped ethernet connection");
        this->state_ = EthernetComponentState::STOPPED;
      } else if (!this->connected_) {
        ESP_LOGW(TAG, "Connection via Ethernet lost! Re-connecting...");
        this->state_ = EthernetComponentState::CONNECTING;
        this->start_connect_();
      }
      break;
  }
}
void EthernetComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Ethernet:");
  this->dump_connect_params_();
  LOG_PIN("  Power Pin: ", this->power_pin_);
  ESP_LOGCONFIG(TAG, "  MDC Pin: %u", this->mdc_pin_);
  ESP_LOGCONFIG(TAG, "  MDIO Pin: %u", this->mdio_pin_);
  ESP_LOGCONFIG(TAG, "  Type: %s", this->type_ == ETHERNET_TYPE_LAN8720 ? "LAN8720" : "TLK110");
}
float EthernetComponent::get_setup_priority() const { return setup_priority::WIFI; }
bool EthernetComponent::can_proceed() { return this->is_connected(); }
network::IPAddress EthernetComponent::get_ip_address() {
  tcpip_adapter_ip_info_t ip;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip);
  return {ip.ip.addr};
}

void EthernetComponent::on_wifi_event_(system_event_id_t event, system_event_info_t info) {
  const char *event_name;

  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      event_name = "ETH started";
      this->started_ = true;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      event_name = "ETH stopped";
      this->started_ = false;
      this->connected_ = false;
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      event_name = "ETH connected";
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      event_name = "ETH disconnected";
      this->connected_ = false;
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      event_name = "ETH Got IP";
      this->connected_ = true;
      break;
    default:
      return;
  }

  ESP_LOGV(TAG, "[Ethernet event] %s (num=%d)", event_name, event);
}

void EthernetComponent::start_connect_() {
  this->connect_begin_ = millis();
  this->status_set_warning();

  esp_err_t err;
  err = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, App.get_name().c_str());
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "tcpip_adapter_set_hostname failed: %s", esp_err_to_name(err));
  }

  tcpip_adapter_ip_info_t info;
  if (this->manual_ip_.has_value()) {
    info.ip.addr = static_cast<uint32_t>(this->manual_ip_->static_ip);
    info.gw.addr = static_cast<uint32_t>(this->manual_ip_->gateway);
    info.netmask.addr = static_cast<uint32_t>(this->manual_ip_->subnet);
  } else {
    info.ip.addr = 0;
    info.gw.addr = 0;
    info.netmask.addr = 0;
  }

  err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH);
  if (err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED) {
    ESPHL_ERROR_CHECK(err, "DHCPC stop error");
  }
  err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &info);
  ESPHL_ERROR_CHECK(err, "DHCPC set IP info error");

  if (this->manual_ip_.has_value()) {
    if (uint32_t(this->manual_ip_->dns1) != 0) {
      ip_addr_t d;
      d.type = IPADDR_TYPE_V4;
      d.u_addr.ip4.addr = static_cast<uint32_t>(this->manual_ip_->dns1);
      dns_setserver(0, &d);
    }
    if (uint32_t(this->manual_ip_->dns1) != 0) {
      ip_addr_t d;
      d.type = IPADDR_TYPE_V4;
      d.u_addr.ip4.addr = static_cast<uint32_t>(this->manual_ip_->dns2);
      dns_setserver(1, &d);
    }
  } else {
    err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH);
    if (err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STARTED) {
      ESPHL_ERROR_CHECK(err, "DHCPC start error");
    }
  }

  this->connect_begin_ = millis();
  this->status_set_warning();
}
void EthernetComponent::eth_phy_config_gpio() {
  phy_rmii_configure_data_interface_pins();
  phy_rmii_smi_configure_pins(global_eth_component->mdc_pin_, global_eth_component->mdio_pin_);
}
void EthernetComponent::eth_phy_power_enable(bool enable) {
  global_eth_component->power_pin_->digital_write(enable);
  // power up takes some time, datasheet says max 300µs
  delay(1);
  global_eth_component->orig_power_enable_fun_(enable);
}
bool EthernetComponent::is_connected() { return this->state_ == EthernetComponentState::CONNECTED; }
void EthernetComponent::dump_connect_params_() {
  tcpip_adapter_ip_info_t ip;
  tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip);
  ESP_LOGCONFIG(TAG, "  IP Address: %s", network::IPAddress(ip.ip.addr).str().c_str());
  ESP_LOGCONFIG(TAG, "  Hostname: '%s'", App.get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Subnet: %s", network::IPAddress(ip.netmask.addr).str().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway: %s", network::IPAddress(ip.gw.addr).str().c_str());

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(3, 3, 4)
  const ip_addr_t *dns_ip1 = dns_getserver(0);
  const ip_addr_t *dns_ip2 = dns_getserver(1);
#else
  ip_addr_t tmp_ip1 = dns_getserver(0);
  const ip_addr_t *dns_ip1 = &tmp_ip1;
  ip_addr_t tmp_ip2 = dns_getserver(1);
  const ip_addr_t *dns_ip2 = &tmp_ip2;
#endif
  ESP_LOGCONFIG(TAG, "  DNS1: %s", network::IPAddress(dns_ip1->u_addr.ip4.addr).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS2: %s", network::IPAddress(dns_ip2->u_addr.ip4.addr).str().c_str());
  uint8_t mac[6];
  esp_eth_get_mac(mac);
  ESP_LOGCONFIG(TAG, "  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  ESP_LOGCONFIG(TAG, "  Is Full Duplex: %s", YESNO(this->eth_config_.phy_get_duplex_mode()));
  ESP_LOGCONFIG(TAG, "  Link Up: %s", YESNO(this->eth_config_.phy_check_link()));
  ESP_LOGCONFIG(TAG, "  Link Speed: %u", this->eth_config_.phy_get_speed_mode() ? 100 : 10);
}
void EthernetComponent::set_phy_addr(uint8_t phy_addr) { this->phy_addr_ = phy_addr; }
void EthernetComponent::set_power_pin(GPIOPin *power_pin) { this->power_pin_ = power_pin; }
void EthernetComponent::set_mdc_pin(uint8_t mdc_pin) { this->mdc_pin_ = mdc_pin; }
void EthernetComponent::set_mdio_pin(uint8_t mdio_pin) { this->mdio_pin_ = mdio_pin; }
void EthernetComponent::set_type(EthernetType type) { this->type_ = type; }
void EthernetComponent::set_clk_mode(eth_clock_mode_t clk_mode) { this->clk_mode_ = clk_mode; }
void EthernetComponent::set_manual_ip(const ManualIP &manual_ip) { this->manual_ip_ = manual_ip; }
std::string EthernetComponent::get_use_address() const {
  if (this->use_address_.empty()) {
    return App.get_name() + ".local";
  }
  return this->use_address_;
}
void EthernetComponent::set_use_address(const std::string &use_address) { this->use_address_ = use_address; }

}  // namespace ethernet
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO

#include "ethernet_component.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

#include <lwip/dns.h>
#include "esp_event.h"

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
  // Delay here to allow power to stabilise before Ethernet is initialised.
  delay(300);  // NOLINT

  esp_err_t err;
  err = esp_netif_init();
  ESPHL_ERROR_CHECK(err, "ETH netif init error");
  err = esp_event_loop_create_default();
  ESPHL_ERROR_CHECK(err, "ETH event loop error");

  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
  this->eth_netif_ = esp_netif_new(&cfg);

  // Init MAC and PHY configs to default
  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

  phy_config.phy_addr = this->phy_addr_;
  if (this->power_pin_ != -1)
    phy_config.reset_gpio_num = this->power_pin_;

  mac_config.smi_mdc_gpio_num = this->mdc_pin_;
  mac_config.smi_mdio_gpio_num = this->mdio_pin_;
  mac_config.clock_config.rmii.clock_mode = this->clk_mode_ == EMAC_CLK_IN_GPIO ? EMAC_CLK_EXT_IN : EMAC_CLK_OUT;
  mac_config.clock_config.rmii.clock_gpio = this->clk_mode_;

  esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);

  esp_eth_phy_t *phy;
  switch (this->type_) {
    case ETHERNET_TYPE_LAN8720: {
      phy = esp_eth_phy_new_lan87xx(&phy_config);
      break;
    }
    case ETHERNET_TYPE_RTL8201: {
      phy = esp_eth_phy_new_rtl8201(&phy_config);
      break;
    }
    case ETHERNET_TYPE_DP83848: {
      phy = esp_eth_phy_new_dp83848(&phy_config);
      break;
    }
    case ETHERNET_TYPE_IP101: {
      phy = esp_eth_phy_new_ip101(&phy_config);
      break;
    }
    case ETHERNET_TYPE_JL1101: {
      phy = esp_eth_phy_new_jl1101(&phy_config);
      break;
    }
    default: {
      this->mark_failed();
      return;
    }
  }

  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
  this->eth_handle_ = nullptr;
  err = esp_eth_driver_install(&eth_config, &this->eth_handle_);
  ESPHL_ERROR_CHECK(err, "ETH driver install error");
  /* attach Ethernet driver to TCP/IP stack */
  err = esp_netif_attach(this->eth_netif_, esp_eth_new_netif_glue(this->eth_handle_));
  ESPHL_ERROR_CHECK(err, "ETH netif attach error");

  // Register user defined event handers
  err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EthernetComponent::eth_event_handler, nullptr);
  ESPHL_ERROR_CHECK(err, "ETH event handler register error");
  err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &EthernetComponent::got_ip_event_handler, nullptr);
  ESPHL_ERROR_CHECK(err, "GOT IP event handler register error");

  /* start Ethernet driver state machine */
  err = esp_eth_start(this->eth_handle_);
  ESPHL_ERROR_CHECK(err, "ETH start error");
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
  std::string eth_type;
  switch (this->type_) {
    case ETHERNET_TYPE_LAN8720:
      eth_type = "LAN8720";
      break;

    case ETHERNET_TYPE_RTL8201:
      eth_type = "RTL8201";
      break;

    case ETHERNET_TYPE_DP83848:
      eth_type = "DP83848";
      break;

    case ETHERNET_TYPE_IP101:
      eth_type = "IP101";
      break;

    default:
      eth_type = "Unknown";
      break;
  }

  ESP_LOGCONFIG(TAG, "Ethernet:");
  this->dump_connect_params_();
  if (this->power_pin_ != -1) {
    ESP_LOGCONFIG(TAG, "  Power Pin: %u", this->power_pin_);
  }
  ESP_LOGCONFIG(TAG, "  MDC Pin: %u", this->mdc_pin_);
  ESP_LOGCONFIG(TAG, "  MDIO Pin: %u", this->mdio_pin_);
  ESP_LOGCONFIG(TAG, "  Type: %s", eth_type.c_str());
}

float EthernetComponent::get_setup_priority() const { return setup_priority::WIFI; }

bool EthernetComponent::can_proceed() { return this->is_connected(); }

network::IPAddress EthernetComponent::get_ip_address() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->eth_netif_, &ip);
  return {ip.ip.addr};
}

void EthernetComponent::eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event, void *event_data) {
  const char *event_name;

  switch (event) {
    case ETHERNET_EVENT_START:
      event_name = "ETH started";
      global_eth_component->started_ = true;
      break;
    case ETHERNET_EVENT_STOP:
      event_name = "ETH stopped";
      global_eth_component->started_ = false;
      global_eth_component->connected_ = false;
      break;
    case ETHERNET_EVENT_CONNECTED:
      event_name = "ETH connected";
      break;
    case ETHERNET_EVENT_DISCONNECTED:
      event_name = "ETH disconnected";
      global_eth_component->connected_ = false;
      break;
    default:
      return;
  }

  ESP_LOGV(TAG, "[Ethernet event] %s (num=%d)", event_name, event);
}

void EthernetComponent::got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                             void *event_data) {
  global_eth_component->connected_ = true;
  ESP_LOGV(TAG, "[Ethernet event] ETH Got IP (num=%d)", event_id);
}

void EthernetComponent::start_connect_() {
  this->connect_begin_ = millis();
  this->status_set_warning();

  esp_err_t err;
  err = esp_netif_set_hostname(this->eth_netif_, App.get_name().c_str());
  if (err != ERR_OK) {
    ESP_LOGW(TAG, "esp_netif_set_hostname failed: %s", esp_err_to_name(err));
  }

  esp_netif_ip_info_t info;
  if (this->manual_ip_.has_value()) {
    info.ip.addr = static_cast<uint32_t>(this->manual_ip_->static_ip);
    info.gw.addr = static_cast<uint32_t>(this->manual_ip_->gateway);
    info.netmask.addr = static_cast<uint32_t>(this->manual_ip_->subnet);
  } else {
    info.ip.addr = 0;
    info.gw.addr = 0;
    info.netmask.addr = 0;
  }

  esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;

  err = esp_netif_dhcpc_get_status(this->eth_netif_, &status);
  ESPHL_ERROR_CHECK(err, "DHCPC Get Status Failed!");

  ESP_LOGV(TAG, "DHCP Client Status: %d", status);

  err = esp_netif_dhcpc_stop(this->eth_netif_);
  if (err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    ESPHL_ERROR_CHECK(err, "DHCPC stop error");
  }

  err = esp_netif_set_ip_info(this->eth_netif_, &info);
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
    err = esp_netif_dhcpc_start(this->eth_netif_);
    if (err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
      ESPHL_ERROR_CHECK(err, "DHCPC start error");
    }
  }

  this->connect_begin_ = millis();
  this->status_set_warning();
}

bool EthernetComponent::is_connected() { return this->state_ == EthernetComponentState::CONNECTED; }

void EthernetComponent::dump_connect_params_() {
  esp_netif_ip_info_t ip;
  esp_netif_get_ip_info(this->eth_netif_, &ip);
  ESP_LOGCONFIG(TAG, "  IP Address: %s", network::IPAddress(ip.ip.addr).str().c_str());
  ESP_LOGCONFIG(TAG, "  Hostname: '%s'", App.get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Subnet: %s", network::IPAddress(ip.netmask.addr).str().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway: %s", network::IPAddress(ip.gw.addr).str().c_str());

  const ip_addr_t *dns_ip1 = dns_getserver(0);
  const ip_addr_t *dns_ip2 = dns_getserver(1);

  ESP_LOGCONFIG(TAG, "  DNS1: %s", network::IPAddress(dns_ip1->u_addr.ip4.addr).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS2: %s", network::IPAddress(dns_ip2->u_addr.ip4.addr).str().c_str());

  esp_err_t err;

  uint8_t mac[6];
  err = esp_eth_ioctl(this->eth_handle_, ETH_CMD_G_MAC_ADDR, &mac);
  ESPHL_ERROR_CHECK(err, "ETH_CMD_G_MAC error");
  ESP_LOGCONFIG(TAG, "  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  eth_duplex_t duplex_mode;
  err = esp_eth_ioctl(this->eth_handle_, ETH_CMD_G_DUPLEX_MODE, &duplex_mode);
  ESPHL_ERROR_CHECK(err, "ETH_CMD_G_DUPLEX_MODE error");
  ESP_LOGCONFIG(TAG, "  Is Full Duplex: %s", YESNO(duplex_mode == ETH_DUPLEX_FULL));

  eth_speed_t speed;
  err = esp_eth_ioctl(this->eth_handle_, ETH_CMD_G_SPEED, &speed);
  ESPHL_ERROR_CHECK(err, "ETH_CMD_G_SPEED error");
  ESP_LOGCONFIG(TAG, "  Link Speed: %u", speed == ETH_SPEED_100M ? 100 : 10);
}

void EthernetComponent::set_phy_addr(uint8_t phy_addr) { this->phy_addr_ = phy_addr; }
void EthernetComponent::set_power_pin(int power_pin) { this->power_pin_ = power_pin; }
void EthernetComponent::set_mdc_pin(uint8_t mdc_pin) { this->mdc_pin_ = mdc_pin; }
void EthernetComponent::set_mdio_pin(uint8_t mdio_pin) { this->mdio_pin_ = mdio_pin; }
void EthernetComponent::set_type(EthernetType type) { this->type_ = type; }
void EthernetComponent::set_clk_mode(emac_rmii_clock_gpio_t clk_mode) { this->clk_mode_ = clk_mode; }
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

#endif  // USE_ESP32

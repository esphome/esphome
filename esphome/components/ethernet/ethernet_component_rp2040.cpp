#include "ethernet_component.h"

#if defined(USE_ETHERNET) && defined(USE_RP2040)

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esphome/components/rp2040/gpio.h"

#include <SPI.h>
#include <lwip/dns.h>
#include <lwip/netif.h>

namespace esphome::ethernet {

static const char *const TAG = "ethernet";

void EthernetComponent::setup() {
  // Configure SPI pins
  SPI.setRX(this->miso_pin_);
  SPI.setTX(this->mosi_pin_);
  SPI.setSCK(this->clk_pin_);

  // Toggle reset pin if configured
  if (this->reset_pin_ >= 0) {
    rp2040::RP2040GPIOPin reset_pin;
    reset_pin.set_pin(this->reset_pin_);
    reset_pin.set_flags(gpio::FLAG_OUTPUT);
    reset_pin.setup();
    reset_pin.digital_write(false);
    delay(1);  // NOLINT
    reset_pin.digital_write(true);
    // W5100S needs 150ms for PLL lock; W5500/ENC28J60 need ~10ms
    delay(RESET_DELAY_MS);  // NOLINT
  }

  // Create the SPI Ethernet device instance
#if defined(USE_ETHERNET_W5500)
  this->eth_ = new Wiznet5500lwIP(this->cs_pin_, SPI, this->interrupt_pin_);  // NOLINT
#elif defined(USE_ETHERNET_W5100)
  this->eth_ = new Wiznet5100lwIP(this->cs_pin_, SPI, this->interrupt_pin_);  // NOLINT
#elif defined(USE_ETHERNET_ENC28J60)
  this->eth_ = new ENC28J60lwIP(this->cs_pin_, SPI, this->interrupt_pin_);  // NOLINT
#endif

  // Set hostname before begin() so the LWIP netif gets it
  this->eth_->hostname(App.get_name().c_str());

  // Configure static IP if set (must be done before begin())
#ifdef USE_ETHERNET_MANUAL_IP
  if (this->manual_ip_.has_value()) {
    IPAddress ip(this->manual_ip_->static_ip);
    IPAddress gateway(this->manual_ip_->gateway);
    IPAddress subnet(this->manual_ip_->subnet);
    IPAddress dns1(this->manual_ip_->dns1);
    IPAddress dns2(this->manual_ip_->dns2);
    this->eth_->config(ip, gateway, subnet, dns1, dns2);
  }
#endif

  // Begin with fixed MAC or auto-generated
  bool success;
  if (this->fixed_mac_.has_value()) {
    success = this->eth_->begin(this->fixed_mac_->data());
  } else {
    success = this->eth_->begin();
  }

  if (!success) {
    ESP_LOGE(TAG, "Failed to initialize Ethernet");
    delete this->eth_;  // NOLINT(cppcoreguidelines-owning-memory)
    this->eth_ = nullptr;
    this->mark_failed();
    return;
  }

  // Make this the default interface for routing
  this->eth_->setDefault(true);

  // The arduino-pico LwipIntfDev automatically handles packet processing
  // via __addEthernetPacketHandler when no interrupt pin is used,
  // or via GPIO interrupt when one is provided.

  // Don't set started_ here — let the link polling in loop() set it
  // when the link is actually up. Setting it prematurely causes
  // a "Starting → Stopped → Starting" log sequence because the chip
  // needs time after begin() before the PHY link is ready.
}

void EthernetComponent::loop() {
  // On RP2040, we need to poll connection state since there are no events.
  const uint32_t now = App.get_loop_component_start_time();

  // Throttle link/IP polling to avoid excessive SPI transactions.
  // W5500/ENC28J60 read PHY register via SPI on every linkStatus() call.
  // W5100 can't detect link state, so we skip the SPI read and assume link-up.
  // connected() reads netif->ip_addr without LwIPLock, but this is a single
  // 32-bit aligned read (atomic on ARM) — worst case is a one-iteration-stale
  // value, which is benign for polling.
  if (this->eth_ != nullptr && now - this->last_link_check_ >= LINK_CHECK_INTERVAL) {
    this->last_link_check_ = now;
#if defined(USE_ETHERNET_W5100)
    // W5100 can't detect link (isLinkDetectable() returns false), so linkStatus()
    // returns Unknown — assume link is up after successful begin()
    bool link_up = true;
#else
    bool link_up = this->eth_->linkStatus() == LinkON;
#endif
    bool has_ip = this->eth_->connected();

    if (!link_up) {
      if (this->started_) {
        this->started_ = false;
        this->connected_ = false;
      }
    } else {
      if (!this->started_) {
        this->started_ = true;
      }
      bool was_connected = this->connected_;
      this->connected_ = has_ip;
      if (this->connected_ && !was_connected) {
#ifdef USE_ETHERNET_IP_STATE_LISTENERS
        this->notify_ip_state_listeners_();
#endif
      }
    }
  }

  // State machine
  switch (this->state_) {
    case EthernetComponentState::STOPPED:
      if (this->started_) {
        ESP_LOGI(TAG, "Starting connection");
        this->state_ = EthernetComponentState::CONNECTING;
        this->start_connect_();
      }
      break;
    case EthernetComponentState::CONNECTING:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped connection");
        this->state_ = EthernetComponentState::STOPPED;
      } else if (this->connected_) {
        // connection established
        ESP_LOGI(TAG, "Connected");
        this->state_ = EthernetComponentState::CONNECTED;

        this->dump_connect_params_();
        this->status_clear_warning();
#ifdef USE_ETHERNET_CONNECT_TRIGGER
        this->connect_trigger_.trigger();
#endif
      } else if (now - this->connect_begin_ > 15000) {
        ESP_LOGW(TAG, "Connecting failed; reconnecting");
        this->start_connect_();
      }
      break;
    case EthernetComponentState::CONNECTED:
      if (!this->started_) {
        ESP_LOGI(TAG, "Stopped connection");
        this->state_ = EthernetComponentState::STOPPED;
#ifdef USE_ETHERNET_DISCONNECT_TRIGGER
        this->disconnect_trigger_.trigger();
#endif
      } else if (!this->connected_) {
        ESP_LOGW(TAG, "Connection lost; reconnecting");
        this->state_ = EthernetComponentState::CONNECTING;
        this->start_connect_();
#ifdef USE_ETHERNET_DISCONNECT_TRIGGER
        this->disconnect_trigger_.trigger();
#endif
      } else {
        this->finish_connect_();
      }
      break;
  }
}

void EthernetComponent::dump_config() {
  const char *type_str = "Unknown";
#if defined(USE_ETHERNET_W5500)
  type_str = "W5500";
#elif defined(USE_ETHERNET_W5100)
  type_str = "W5100";
#elif defined(USE_ETHERNET_ENC28J60)
  type_str = "ENC28J60";
#endif
  ESP_LOGCONFIG(TAG,
                "Ethernet:\n"
                "  Type: %s\n"
                "  Connected: %s\n"
                "  CLK Pin: %u\n"
                "  MISO Pin: %u\n"
                "  MOSI Pin: %u\n"
                "  CS Pin: %u\n"
                "  IRQ Pin: %d\n"
                "  Reset Pin: %d",
                type_str, YESNO(this->is_connected()), this->clk_pin_, this->miso_pin_, this->mosi_pin_, this->cs_pin_,
                this->interrupt_pin_, this->reset_pin_);
  this->dump_connect_params_();
}

network::IPAddresses EthernetComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  if (this->eth_ != nullptr) {
    LwIPLock lock;
    addresses[0] = network::IPAddress(this->eth_->localIP());
  }
  return addresses;
}

network::IPAddress EthernetComponent::get_dns_address(uint8_t num) {
  LwIPLock lock;
  const ip_addr_t *dns_ip = dns_getserver(num);
  return dns_ip;
}

void EthernetComponent::get_eth_mac_address_raw(uint8_t *mac) {
  if (this->eth_ != nullptr) {
    this->eth_->macAddress(mac);
  } else {
    memset(mac, 0, 6);
  }
}

std::string EthernetComponent::get_eth_mac_address_pretty() {
  char buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  return std::string(this->get_eth_mac_address_pretty_into_buffer(buf));
}

const char *EthernetComponent::get_eth_mac_address_pretty_into_buffer(
    std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buf) {
  uint8_t mac[6];
  get_eth_mac_address_raw(mac);
  format_mac_addr_upper(mac, buf.data());
  return buf.data();
}

eth_duplex_t EthernetComponent::get_duplex_mode() {
  // W5100, W5500, and ENC28J60 are full-duplex on RP2040
  return ETH_DUPLEX_FULL;
}

eth_speed_t EthernetComponent::get_link_speed() {
#ifdef USE_ETHERNET_ENC28J60
  // ENC28J60 is 10Mbps only
  return ETH_SPEED_10M;
#else
  // W5100 and W5500 are 100Mbps
  return ETH_SPEED_100M;
#endif
}

bool EthernetComponent::powerdown() {
  ESP_LOGI(TAG, "Powering down ethernet");
  if (this->eth_ != nullptr) {
    this->eth_->end();
  }
  this->connected_ = false;
  this->started_ = false;
  return true;
}

void EthernetComponent::start_connect_() {
  this->got_ipv4_address_ = false;
  this->connect_begin_ = millis();
  this->status_set_warning(LOG_STR("waiting for IP configuration"));

  // Hostname is already set in setup() via LwipIntf::setHostname()

#ifdef USE_ETHERNET_MANUAL_IP
  if (this->manual_ip_.has_value()) {
    // Static IP was already configured before begin() in setup()
    // Set DNS servers
    LwIPLock lock;
    if (this->manual_ip_->dns1.is_set()) {
      ip_addr_t d;
      d = this->manual_ip_->dns1;
      dns_setserver(0, &d);
    }
    if (this->manual_ip_->dns2.is_set()) {
      ip_addr_t d;
      d = this->manual_ip_->dns2;
      dns_setserver(1, &d);
    }
  }
#endif
}

void EthernetComponent::finish_connect_() {
  // No additional work needed on RP2040 for now
  // IPv6 link-local could be added here in the future
}

void EthernetComponent::dump_connect_params_() {
  if (this->eth_ == nullptr) {
    return;
  }

  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char subnet_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char gateway_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char dns1_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char dns2_buf[network::IP_ADDRESS_BUFFER_SIZE];
  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];

  // Copy all lwIP state under the lock to avoid races with IRQ callbacks
  ip_addr_t ip_addr, netmask, gw, dns1_addr, dns2_addr;
  {
    LwIPLock lock;
    auto *netif = this->eth_->getNetIf();
    ip_addr = netif->ip_addr;
    netmask = netif->netmask;
    gw = netif->gw;
    dns1_addr = *dns_getserver(0);
    dns2_addr = *dns_getserver(1);
  }
  ESP_LOGCONFIG(TAG,
                "  IP Address: %s\n"
                "  Hostname: '%s'\n"
                "  Subnet: %s\n"
                "  Gateway: %s\n"
                "  DNS1: %s\n"
                "  DNS2: %s\n"
                "  MAC Address: %s",
                network::IPAddress(&ip_addr).str_to(ip_buf), App.get_name().c_str(),
                network::IPAddress(&netmask).str_to(subnet_buf), network::IPAddress(&gw).str_to(gateway_buf),
                network::IPAddress(&dns1_addr).str_to(dns1_buf), network::IPAddress(&dns2_addr).str_to(dns2_buf),
                this->get_eth_mac_address_pretty_into_buffer(mac_buf));
}

void EthernetComponent::set_clk_pin(uint8_t clk_pin) { this->clk_pin_ = clk_pin; }
void EthernetComponent::set_miso_pin(uint8_t miso_pin) { this->miso_pin_ = miso_pin; }
void EthernetComponent::set_mosi_pin(uint8_t mosi_pin) { this->mosi_pin_ = mosi_pin; }
void EthernetComponent::set_cs_pin(uint8_t cs_pin) { this->cs_pin_ = cs_pin; }
void EthernetComponent::set_interrupt_pin(int8_t interrupt_pin) { this->interrupt_pin_ = interrupt_pin; }
void EthernetComponent::set_reset_pin(int8_t reset_pin) { this->reset_pin_ = reset_pin; }

}  // namespace esphome::ethernet

#endif  // USE_ETHERNET && USE_RP2040

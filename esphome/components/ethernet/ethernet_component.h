#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/ip_address.h"

#ifdef USE_ESP32

#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_netif.h"
#include "esp_mac.h"

namespace esphome {
namespace ethernet {

enum EthernetType {
  ETHERNET_TYPE_UNKNOWN = 0,
  ETHERNET_TYPE_LAN8720,
  ETHERNET_TYPE_RTL8201,
  ETHERNET_TYPE_DP83848,
  ETHERNET_TYPE_IP101,
  ETHERNET_TYPE_JL1101,
  ETHERNET_TYPE_KSZ8081,
  ETHERNET_TYPE_KSZ8081RNA,
  ETHERNET_TYPE_W5500,
};

struct ManualIP {
  network::IPAddress static_ip;
  network::IPAddress gateway;
  network::IPAddress subnet;
  network::IPAddress dns1;  ///< The first DNS server. 0.0.0.0 for default.
  network::IPAddress dns2;  ///< The second DNS server. 0.0.0.0 for default.
};

struct PHYRegister {
  uint32_t address;
  uint32_t value;
  uint32_t page;
};

enum class EthernetComponentState {
  STOPPED,
  CONNECTING,
  CONNECTED,
};

class EthernetComponent : public Component {
 public:
  EthernetComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  bool can_proceed() override;
  void on_shutdown() override { powerdown(); }
  bool is_connected();

#ifdef USE_ETHERNET_SPI
  void set_clk_pin(uint8_t clk_pin);
  void set_miso_pin(uint8_t miso_pin);
  void set_mosi_pin(uint8_t mosi_pin);
  void set_cs_pin(uint8_t cs_pin);
  void set_interrupt_pin(uint8_t interrupt_pin);
  void set_reset_pin(uint8_t reset_pin);
  void set_clock_speed(int clock_speed);
#else
  void set_phy_addr(uint8_t phy_addr);
  void set_power_pin(int power_pin);
  void set_mdc_pin(uint8_t mdc_pin);
  void set_mdio_pin(uint8_t mdio_pin);
  void set_clk_mode(emac_rmii_clock_mode_t clk_mode, emac_rmii_clock_gpio_t clk_gpio);
  void add_phy_register(PHYRegister register_value);
#endif
  void set_type(EthernetType type);
  void set_manual_ip(const ManualIP &manual_ip);

  network::IPAddresses get_ip_addresses();
  network::IPAddress get_dns_address(uint8_t num);
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);
  void get_eth_mac_address_raw(uint8_t *mac);
  std::string get_eth_mac_address_pretty();
  eth_duplex_t get_duplex_mode();
  eth_speed_t get_link_speed();
  bool powerdown();

 protected:
  static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
#if LWIP_IPV6
  static void got_ip6_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
#endif /* LWIP_IPV6 */

  void start_connect_();
  void dump_connect_params_();
  /// @brief Set `RMII Reference Clock Select` bit for KSZ8081.
  void ksz8081_set_clock_reference_(esp_eth_mac_t *mac);
  /// @brief Set arbitratry PHY registers from config.
  void write_phy_register_(esp_eth_mac_t *mac, PHYRegister register_data);

  std::string use_address_;
#ifdef USE_ETHERNET_SPI
  uint8_t clk_pin_;
  uint8_t miso_pin_;
  uint8_t mosi_pin_;
  uint8_t cs_pin_;
  uint8_t interrupt_pin_;
  int reset_pin_{-1};
  int phy_addr_spi_{-1};
  int clock_speed_;
#else
  uint8_t phy_addr_{0};
  int power_pin_{-1};
  uint8_t mdc_pin_{23};
  uint8_t mdio_pin_{18};
  emac_rmii_clock_mode_t clk_mode_{EMAC_CLK_EXT_IN};
  emac_rmii_clock_gpio_t clk_gpio_{EMAC_CLK_IN_GPIO};
  std::vector<PHYRegister> phy_registers_{};
#endif
  EthernetType type_{ETHERNET_TYPE_UNKNOWN};
  optional<ManualIP> manual_ip_{};

  bool started_{false};
  bool connected_{false};
  bool got_ipv4_address_{false};
#if LWIP_IPV6
  uint8_t ipv6_count_{0};
#endif /* LWIP_IPV6 */
  EthernetComponentState state_{EthernetComponentState::STOPPED};
  uint32_t connect_begin_;
  esp_netif_t *eth_netif_{nullptr};
  esp_eth_handle_t eth_handle_;
  esp_eth_phy_t *phy_{nullptr};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern EthernetComponent *global_eth_component;
extern "C" esp_eth_phy_t *esp_eth_phy_new_jl1101(const eth_phy_config_t *config);

}  // namespace ethernet
}  // namespace esphome

#endif  // USE_ESP32

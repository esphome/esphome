#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/ip_address.h"

#include "ETH.h"

namespace esphome {
namespace ethernet {

struct ManualIP {
  network::IPAddress static_ip;
  network::IPAddress gateway;
  network::IPAddress subnet;
  network::IPAddress dns1;  ///< The first DNS server. 0.0.0.0 for default.
  network::IPAddress dns2;  ///< The second DNS server. 0.0.0.0 for default.
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
  bool is_connected();

  void set_phy_addr(uint8_t phy_addr);
  void set_power_pin(GPIOPin *power_pin);
  void set_mdc_pin(uint8_t mdc_pin);
  void set_mdio_pin(uint8_t mdio_pin);
  void set_type(eth_phy_type_t type);
  void set_clk_mode(eth_clock_mode_t clk_mode);
  void set_manual_ip(const ManualIP &manual_ip);

  network::IPAddress get_ip_address();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);

 protected:
  static void on_wifi_event_(WiFiEvent_t event);
  void start_connect_();
  void dump_connect_params_();

  std::string use_address_;
  uint8_t phy_addr_{0};
  GPIOPin *power_pin_{nullptr};
  uint8_t mdc_pin_{23};
  uint8_t mdio_pin_{18};
  eth_phy_type_t type_{ETH_PHY_LAN8720};
  eth_clock_mode_t clk_mode_{ETH_CLOCK_GPIO0_IN};
  optional<ManualIP> manual_ip_{};

  bool started_{false};
  bool connected_{false};
  EthernetComponentState state_{EthernetComponentState::STOPPED};
  uint32_t connect_begin_;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern EthernetComponent *global_eth_component;

}  // namespace ethernet
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO

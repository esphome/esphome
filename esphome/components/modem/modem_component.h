#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/ip_address.h"

#ifdef USE_ESP32

#include "esp_netif.h"

namespace esphome {
namespace modem {

enum ModemType {
  MODEM_TYPE_UNKNOWN = 0,
  MODEM_TYPE_BG96,
  MODEM_TYPE_SIM800,
  MODEM_TYPE_SIM7000,
  MODEM_TYPE_SIM7070,
};

struct ManualIP {
  network::IPAddress static_ip;
  network::IPAddress gateway;
  network::IPAddress subnet;
  network::IPAddress dns1;  ///< The first DNS server. 0.0.0.0 for default.
  network::IPAddress dns2;  ///< The second DNS server. 0.0.0.0 for default.
};

enum class ModemComponentState {
  STOPPED,
  CONNECTING,
  CONNECTED,
};

class ModemComponent : public Component {
 public:
  ModemComponent();
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  bool can_proceed() override;
  void on_shutdown() override { powerdown(); }
  bool is_connected();

  void set_power_pin(int power_pin);
  void set_type(ModemType type);
  void set_reset_pin(int reset_pin);
  //void set_clk_mode(emac_rmii_clock_mode_t clk_mode, emac_rmii_clock_gpio_t clk_gpio);
  //void set_manual_ip(const ManualIP &manual_ip);

  network::IPAddress get_ip_address();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);
  bool powerdown();

 protected:
  static void modem_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

  void start_connect_();
  void esp_modem_hard_reset();

  std::string use_address_;
  int power_pin_{-1};
  int reset_pin_{-1};
  ModemType type_{MODEM_TYPE_UNKNOWN};
  optional<ManualIP> manual_ip_{};

  bool started_{false};
  bool connected_{false};

  ModemComponentState state_{ModemComponentState::STOPPED};
  uint32_t connect_begin_;
  esp_netif_t *modem_netif_{nullptr};
  //esp_eth_phy_t *phy_{nullptr};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ModemComponent *global_modem_component;

}  // namespace modem
}  // namespace esphome

#endif  // USE_ESP32

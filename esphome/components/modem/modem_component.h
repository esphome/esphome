#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/ip_address.h"

#ifdef USE_ESP32

using esphome::esp_log_printf_;

#include "esp_netif.h"
#include "cxx_include/esp_modem_api.hpp"

namespace esphome {
namespace modem {

enum ModemType {
  MODEM_TYPE_UNKNOWN = 0,
  MODEM_TYPE_BG96,
  MODEM_TYPE_SIM800,
  MODEM_TYPE_SIM7000,
  MODEM_TYPE_SIM7070,
};

enum class ModemComponentState {
  STOPPED,
  RESETTING,
  TURNING_ON,
  TURNING_OFF,
  CONNECTING,
  CONNECTED,
};

class ModemComponent : public Component {
 public:
  ModemComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;
  void dump_connect_params();
  float get_setup_priority() const override;
  bool can_proceed() override;
  bool is_connected();
  void set_power_pin(InternalGPIOPin *power_pin);
  void set_pwrkey_pin(InternalGPIOPin *pwrkey_pin);
  void set_type(ModemType type);
  void set_reset_pin(InternalGPIOPin *reset_pin);
  void set_apn(const std::string &apn);
  void set_tx_pin(int tx_pin);
  void set_rx_pin(int rx_pin);
  void set_uart_event_task_stack_size(int uart_event_task_stack_size);
  void set_uart_event_task_priority(int uart_event_task_priority);
  void set_uart_event_queue_size(int uart_event_queue_size);
  void set_uart_tx_buffer_size(int uart_tx_buffer_size);
  void set_uart_rx_buffer_size(int uart_rx_buffer_size);

  network::IPAddress get_ip_address();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);
  bool powerdown();

 protected:
  static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

  bool turn_on_modem();
  bool turn_off_modem();
  bool use_pwrkey();
  void start_connect_();
  void reset_modem();
  int get_rssi();
  int get_modem_voltage();

  std::shared_ptr<esp_modem::DTE> dte{nullptr};
  std::unique_ptr<esp_modem::DCE> dce{nullptr};
  ModemType type_{MODEM_TYPE_UNKNOWN};
  InternalGPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *power_pin_{nullptr};
  InternalGPIOPin *pwrkey_pin_{nullptr};
  int tx_pin_{-1};
  int rx_pin_{-1};
  std::string apn_{""};
  std::string use_address_;
  int uart_event_task_stack_size_{0};
  int uart_event_task_priority_{0};
  int uart_event_queue_size_{0};
  int uart_tx_buffer_size_{0};
  int uart_rx_buffer_size_{0};

  bool started_{false};
  bool connected_{false};

  ModemComponentState state_{ModemComponentState::STOPPED};
  uint32_t connect_begin_;
  esp_netif_t *modem_netif_{nullptr};
  // esp_eth_phy_t *phy_{nullptr};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ModemComponent *global_modem_component;

}  // namespace modem
}  // namespace esphome

#endif  // USE_ESP32

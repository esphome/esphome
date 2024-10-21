#pragma once
#ifdef USE_ESP32
#ifdef USE_ESP_IDF

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/ip_address.h"

#include <map>
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
  TURNING_ON_POWER,
  TURNING_OFF_POWER,
  TURNING_ON_PWRKEY,
  TURNING_OFF_PWRKEY,
  TURNING_ON_RESET,
  TURNING_OFF_RESET,
  SYNC,
  REGISTRATION_IN_NETWORK,
  CONNECTING,
  CONNECTED,
};

struct ModemComponentStateTiming {
  uint poll_period;
  uint time_limit;
  ModemComponentStateTiming() : poll_period(0), time_limit(0) {}
  ModemComponentStateTiming(int poll_period, int time_limit) : poll_period(poll_period), time_limit(time_limit) {}
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
  void set_tx_pin(uint8_t tx_pin);
  void set_rx_pin(uint8_t rx_pin);
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
  std::map<ModemComponentState, ModemComponentStateTiming> modem_component_state_timing_map_ = {
      {ModemComponentState::TURNING_ON_POWER, ModemComponentStateTiming(2000, 0)},
      {ModemComponentState::TURNING_OFF_POWER, ModemComponentStateTiming(2000, 0)},
      {ModemComponentState::TURNING_ON_PWRKEY, ModemComponentStateTiming(2000, 0)},
      {ModemComponentState::TURNING_OFF_PWRKEY, ModemComponentStateTiming(2000, 0)},
      {ModemComponentState::TURNING_ON_RESET, ModemComponentStateTiming(2000, 0)},
      {ModemComponentState::TURNING_OFF_RESET, ModemComponentStateTiming(2000, 0)},
      {ModemComponentState::SYNC, ModemComponentStateTiming(2000, 30000)},
      {ModemComponentState::REGISTRATION_IN_NETWORK, ModemComponentStateTiming(2000, 30000)},
      {ModemComponentState::CONNECTING, ModemComponentStateTiming(2000, 60000)},
      {ModemComponentState::CONNECTED, ModemComponentStateTiming(5000, 0)},
  };

  static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int event_id, void *event_data);
  void modem_netif_init_();
  void dte_init_();
  void dce_init_();

  bool check_modem_component_state_timings_();
  int get_rssi_();
  int get_modem_voltage_();
  const char *get_state_();
  void set_state_(ModemComponentState state);
  const char *state_to_string_(ModemComponentState state);

  std::shared_ptr<esp_modem::DTE> dte_{nullptr};
  std::unique_ptr<esp_modem::DCE> dce_{nullptr};
  ModemType type_{MODEM_TYPE_UNKNOWN};
  InternalGPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *power_pin_{nullptr};
  InternalGPIOPin *pwrkey_pin_{nullptr};
  uint8_t tx_pin_{0};
  uint8_t rx_pin_{0};
  std::string apn_{""};
  std::string use_address_;
  int uart_event_task_stack_size_{0};
  int uart_event_task_priority_{0};
  int uart_event_queue_size_{0};
  int uart_tx_buffer_size_{0};
  int uart_rx_buffer_size_{0};

  uint pull_time_{0};
  uint change_state_{0};

  bool started_{false};

  ModemComponentState state_{ModemComponentState::TURNING_ON_POWER};
  int connect_begin_;
  esp_netif_t *modem_netif_{nullptr};
  // esp_eth_phy_t *phy_{nullptr};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ModemComponent *global_modem_component;

}  // namespace modem
}  // namespace esphome

#endif  // USE_ESP_IDF
#endif  // USE_ESP32

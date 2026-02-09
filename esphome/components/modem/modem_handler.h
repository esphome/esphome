
#pragma once
#ifdef USE_ESP_IDF

#include <memory>
#include "esphome/core/gpio.h"
#include <cxx_include/esp_modem_api.hpp>
#include <esp_modem_config.h>
#include <vector>
#include <cmath>

namespace esphome {
namespace modem {

using namespace esp_modem;

struct AtCommandResult {
  std::string output{};
  bool success{false};
  command_result esp_modem_command_result{command_result::TIMEOUT};
  operator bool() const { return success; }
  const char *c_str() const { return output.c_str(); }
};

struct NetworkInfos {
  esp_netif_ip_info_t ip_info{};
  esp_netif_dns_info_t dns_main{};
  esp_netif_dns_info_t dns_backup{};
  bool got_ip{false};
};

class ModemHandler {
 public:
  // Attributes from YAML config
  InternalGPIOPin *tx_pin;
  InternalGPIOPin *rx_pin;
  InternalGPIOPin *rts_pin{nullptr};
  InternalGPIOPin *cts_pin{nullptr};
  std::string model;
  GPIOPin *status_pin{nullptr};
  uint32_t power_ton_pulse_delay;
  uint32_t power_ton_delay;
  uint32_t power_toff_pulse_delay;
  uint32_t power_toff_delay;
  GPIOPin *power_pin{nullptr};
  bool power_pin_inverted{false};
  uint16_t tx_buffer_size = 512;
  uint16_t rx_buffer_size = 512;
  uint16_t dte_buffer_size = 512;
  std::string pin_code;
  std::string apn;
  std::vector<std::string> init_at_commands;
  int baud_rate = 0;
  bool cmux{false};

  // esp_modem objects
  std::unique_ptr<DCE> dce{nullptr};
  std::shared_ptr<DTE> dte{nullptr};
  esp_netif_t *ppp_netif{nullptr};

  // Internal state attributes
  int current_baud_rate{0};
  NetworkInfos network_infos{};

  // UART config
  uint8_t uart_event_queue_size = 10;
  size_t uart_event_task_stack_size = 4096;
  uint8_t uart_event_task_priority = 5;
  uint32_t command_delay = 1000;
  uint32_t connect_retry_delay = 5000;
  uart_port_t uart_port_num = UART_NUM_MAX;

  // Methods
  void modem_create_dte_dce(int baud_rate);
  void enable_debug();
  AtCommandResult send_at(const std::string &cmd, uint32_t timeout = 0, bool verbose = false);
  bool get_power_status();
  bool get_signal_quality(float &out_rssi, float &out_ber);
  static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
  void modem_log_status();
  // std::string modem_network_status_string();
  void send_init_at();
  // void update_network_state();
  bool prepare_sim();
};

}  // namespace modem
}  // namespace esphome
#endif

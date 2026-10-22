#ifdef USE_ESP32

#include "modem_handler.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cmath>

namespace esphome::modem {

using namespace esp_modem;

ESPHOME_LOG_TAG(TAG, "modem_handler");

void ModemHandler::modem_create_dte_dce(int baud_rate) {
  this->current_baud_rate = baud_rate;

  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();

  dte_config.uart_config.tx_io_num = this->tx_pin->get_pin();
  dte_config.uart_config.rx_io_num = this->rx_pin->get_pin();
  dte_config.uart_config.rx_buffer_size = this->rx_buffer_size;
  dte_config.uart_config.tx_buffer_size = this->tx_buffer_size;
  dte_config.uart_config.event_queue_size = this->uart_event_queue_size;
  dte_config.uart_config.port_num = this->uart_port_num;
  if (baud_rate != 0) {
    ESP_LOGD(TAG, "DTE baud rate: %d", baud_rate);
    dte_config.uart_config.baud_rate = baud_rate;
  }
  if (this->rts_pin != nullptr && this->cts_pin != nullptr) {
    ESP_LOGD(TAG, "Using RTS/CTS hardware flow control");
    dte_config.uart_config.rts_io_num = this->rts_pin->get_pin();
    dte_config.uart_config.cts_io_num = this->cts_pin->get_pin();
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_HW;
  } else {
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;
  }
  dte_config.task_stack_size = this->uart_event_task_stack_size;
  dte_config.task_priority = this->uart_event_task_priority;
  dte_config.dte_buffer_size = this->dte_buffer_size;

  this->dce.reset();
  this->dte.reset();
  this->dte = create_uart_dte(&dte_config);

  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(this->apn.c_str());

  if (this->model == "GENERIC") {
    this->dce = create_generic_dce(&dce_config, this->dte, this->ppp_netif);
  } else if (this->model == "BG96") {
    this->dce = create_BG96_dce(&dce_config, this->dte, this->ppp_netif);
  } else if (this->model == "SIM800") {
    this->dce = create_SIM800_dce(&dce_config, this->dte, this->ppp_netif);
  } else if (this->model == "SIM7000" || this->model == "SIM7080") {
    this->dce = create_SIM7000_dce(&dce_config, this->dte, this->ppp_netif);
  } else if (this->model == "SIM7070") {
    this->dce = create_SIM7070_dce(&dce_config, this->dte, this->ppp_netif);
  } else if (this->model == "SIM7600" || this->model == "SIM7670") {
    this->dce = create_SIM7600_dce(&dce_config, this->dte, this->ppp_netif);
  } else {
    ESP_LOGE(TAG, "Invalid model %s. DCE not created.", this->model.c_str());
    return;
  }

  ESP_LOGV(TAG, "DTE/DCE created.");
}

AtCommandResult ModemHandler::send_at(const std::string &cmd, uint32_t timeout, bool verbose) {
  const uint32_t cmd_timeout = timeout == 0 ? this->command_delay : timeout;
  AtCommandResult at_command_result;
  at_command_result.success = false;
  at_command_result.esp_modem_command_result = command_result::TIMEOUT;
  if (this->dce) {
    at_command_result.esp_modem_command_result = this->dce->at(cmd, at_command_result.output, cmd_timeout);
    at_command_result.success = at_command_result.esp_modem_command_result == command_result::OK;
    if (verbose) {
      ESP_LOGI(TAG, "AT '%s' -> %s: %s", cmd.c_str(),
               at_command_result.success ? LOG_STR_LITERAL("OK") : LOG_STR_LITERAL("FAILED"),
               at_command_result.output.c_str());
    }
  } else {
    ESP_LOGE(TAG, "Modem DCE not ready, cannot send AT command: %s", cmd.c_str());
    at_command_result.esp_modem_command_result = command_result::FAIL;
  }
  return at_command_result;
}

bool ModemHandler::get_signal_quality(float &out_rssi, float &out_ber) {
  out_rssi = NAN;
  out_ber = NAN;
  int modem_rssi = 99;
  int modem_ber = 99;
  if (this->dce && this->dce->sync() == command_result::OK &&
      (this->dce->get_signal_quality(modem_rssi, modem_ber) == command_result::OK)) {
    if (modem_rssi != 99)
      out_rssi = -113.0f + (modem_rssi * 2);
    if (modem_ber != 99)
      out_ber = 0.1f * (modem_ber * modem_ber);
    return true;
  }
  return false;
}

void ModemHandler::modem_log_status() {
  bool synced = false;
  int cfun = -1;
  int attached = 0;
  int network_mode = 0;
  int registration_state = 0;
  float rssi = NAN, ber = NAN;
  std::string sim_status = "UNKNOWN";
  if (this->dce && this->dce->sync() == esp_modem::command_result::OK) {
    synced = true;
    auto res = this->send_at("AT+CPIN?");
    if (res) {
      auto &out = res.output;
      sim_status = out.starts_with("+CPIN: ") ? out.substr(7) : out;
    }
    this->dce->get_radio_state(cfun);
    this->dce->get_network_attachment_state(attached);
    this->dce->get_network_registration_state(registration_state);
    this->get_signal_quality(rssi, ber);
    this->dce->get_network_system_mode(network_mode);
  }
  bool connected =
      synced && network_mode != 0 && attached && !std::isnan(rssi) && sim_status.find("READY") != std::string::npos;
  char cfun_str[16];
  if (cfun == 1) {
    snprintf(cfun_str, sizeof(cfun_str), "OK");
  } else {
    snprintf(cfun_str, sizeof(cfun_str), "NOK(%d)", cfun);
  }
  ESP_LOGI(TAG,
           "Modem status: %s, attached: %s, registration state: %d, radio function: %s, SIM: %s, type: %d, ber: %.0f "
           "%%, rssi: %.0f dB",
           connected ? LOG_STR_LITERAL("Good") : (synced ? LOG_STR_LITERAL("BAD") : LOG_STR_LITERAL("No SYNC")),
           attached ? LOG_STR_LITERAL("Yes") : LOG_STR_LITERAL("NO"), registration_state, cfun_str, sim_status.c_str(),
           (int) network_mode, ber * 100.0f, rssi);
}

SimResult ModemHandler::prepare_sim() {
  App.feed_wdt();
  delay(200);  // NOLINT
  AtCommandResult result = this->send_at("AT+CPIN?");
  if (!result.success) {
    ESP_LOGW(TAG, "Failed to check pin: %s", result.c_str());
    return SimResult::SIM_RESULT_RETRY;
  }

  std::string current_sim_status = result.output.starts_with("+CPIN: ") ? result.output.substr(7) : result.output;
  ESP_LOGD(TAG, "SIM: %s", current_sim_status.c_str());

  if (current_sim_status.find("READY") != std::string::npos) {
    return SimResult::SIM_RESULT_READY;
  }
  if (current_sim_status.find("PUK") != std::string::npos) {
    ESP_LOGE(TAG, "SIM is PUK locked; unlock it in a phone");
    return SimResult::SIM_RESULT_FATAL;
  }
  if (current_sim_status.find("PIN") == std::string::npos) {
    return SimResult::SIM_RESULT_RETRY;  // e.g. the SIM is still initializing
  }
  if (this->pin_code.empty()) {
    ESP_LOGE(TAG, "SIM requires a PIN but pin_code is not set");
    return SimResult::SIM_RESULT_FATAL;
  }
  if (this->pin_sent) {
    ESP_LOGE(TAG, "SIM still locked after sending the PIN; check pin_code");
    return SimResult::SIM_RESULT_FATAL;
  }
  this->pin_sent = true;
  ESP_LOGD(TAG, "Unlocking SIM with PIN...");
  if (this->dce->set_pin(this->pin_code) != command_result::OK) {
    ESP_LOGE(TAG, "The modem rejected the PIN; check pin_code");
    return SimResult::SIM_RESULT_FATAL;
  }
  return SimResult::SIM_RESULT_RETRY;  // AT+CPIN? confirms READY on the next pass
}

void ModemHandler::ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  auto *handler = static_cast<ModemHandler *>(arg);

  switch (event_id) {
    case IP_EVENT_PPP_GOT_IP: {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
      ESP_LOGD(TAG, "PPP IP event: Got IP " IPSTR, IP2STR(&event->ip_info.ip));
      handler->network_infos.ip_info = event->ip_info;
      esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_MAIN, &handler->network_infos.dns_main);
      esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_BACKUP, &handler->network_infos.dns_backup);
      handler->network_infos.got_ip.store(true, std::memory_order_release);
      break;
    }
    case IP_EVENT_PPP_LOST_IP:
      if (handler->network_infos.got_ip.load(std::memory_order_acquire)) {
        ESP_LOGD(TAG, "PPP IP event: Lost IP.");
        handler->network_infos.got_ip.store(false, std::memory_order_release);
      }
      break;
  }
}

}  // namespace esphome::modem
#endif

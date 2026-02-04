#ifdef USE_ESP_IDF

#include "modem_handler.h"
#include "helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <cmath>

namespace esphome {
namespace modem {

using namespace esp_modem;

static const char *const TAG = "modem_handler";

#define ESPMODEM_ERROR_CHECK(err, message) \
  if ((err) != command_result::OK) { \
    ESP_LOGE(TAG, message ": %s", command_result_to_string(err).c_str()); \
  }

// Define a static free function for URC handling
esp_modem::DTE::UrcConsumeInfo static_urc_handler(const esp_modem::DTE::UrcBufferInfo &buffer_info) {
  if (!buffer_info.is_command_active) {
    std::string line(reinterpret_cast<const char *>(buffer_info.new_data_start), buffer_info.new_data_size);
    ESP_LOGD(TAG, "Modem URC: %s", line.c_str());
    // Handle URC lines here if needed
    // Note: This static function does not have access to `this` (ModemHandler instance).
    // If `this` is required, a different approach (e.g., passing a pointer via user_data if API allows)
    // would be necessary, or reconsidering the use of std::bind with a member function.
    // return esp_modem::DTE::UrcConsumeInfo{esp_modem::DTE::UrcConsumeResult::CONSUME_ALL, SIZE_MAX};
    // return esp_modem::DTE::UrcConsumeInfo{esp_modem::DTE::UrcConsumeResult::CONSUME_PARTIAL,
    // buffer_info.new_data_size};
  }
  return esp_modem::DTE::UrcConsumeInfo{esp_modem::DTE::UrcConsumeResult::CONSUME_NONE, 0};
}

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
  this->dce->set_enhanced_urc(static_urc_handler);
  ESP_LOGV(TAG, "DTE/DCE created.");
}

void ModemHandler::enable_debug() { esp_log_level_set("command_lib", ESP_LOG_VERBOSE); }

AtCommandResult ModemHandler::send_at(const std::string &cmd, uint32_t timeout, bool verbose) {
  const uint32_t cmd_timeout = timeout == 0 ? this->command_delay : timeout;
  AtCommandResult at_command_result;
  at_command_result.success = false;
  at_command_result.esp_modem_command_result = command_result::TIMEOUT;
  if (this->dce) {
    at_command_result.esp_modem_command_result = this->dce->at(cmd, at_command_result.output, cmd_timeout);
    at_command_result.success = at_command_result.esp_modem_command_result == command_result::OK;

    auto level = ESPHOME_LOG_LEVEL_VERY_VERBOSE;
    if (verbose) {
      if (at_command_result.success) {
        level = ESPHOME_LOG_LEVEL_INFO;
      } else {
        level = ESPHOME_LOG_LEVEL_ERROR;
      }
    }

    esp_log_printf_(level, TAG, __LINE__, "Result for command %s: %s (status %s)", cmd.c_str(),
                    at_command_result.c_str(),
                    command_result_to_string(at_command_result.esp_modem_command_result).c_str());
  } else {
    ESP_LOGE(TAG, "Modem DCE not ready, cannot send AT command: %s", cmd.c_str());
    at_command_result.esp_modem_command_result = command_result::FAIL;
  }
  return at_command_result;
}

bool ModemHandler::get_power_status() {
  if (this->status_pin) {
    return this->status_pin->digital_read();
  }
  if (this->dce && this->dce->sync() == command_result::OK) {
    return true;
  }
  ESP_LOGW(TAG, "No status pin, modem sync failed. Assuming powered on.");
  return true;
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
  std::string cfun_str = (cfun == 1) ? "OK" : "NOK(" + std::to_string(cfun) + ")";

  ESP_LOGI(TAG,
           "Modem status: %s, attached: %s, registration state: %d,radio function: %s, SIM: %s, type: %s, ber: %.0f%%, "
           "rssi: %.0fdB %s",
           connected ? "Good" : (synced ? "BAD" : "No SYNC"), attached ? "Yes" : "NO", registration_state,
           cfun_str.c_str(), sim_status.c_str(), network_system_mode_to_string(network_mode).c_str(), ber * 100.0f,
           rssi, get_signal_bars(rssi).c_str());
}

void ModemHandler::send_init_at() {
  for (const auto &cmd : this->init_at_commands) {
    App.feed_wdt();
    AtCommandResult result = this->send_at(cmd);
    if (result.success) {
      ESP_LOGI(TAG, "init_at %s: %s", cmd.c_str(), result.output.c_str());
    } else {
      ESP_LOGW(TAG, "init_at %s: %s", cmd.c_str(), command_result_to_string(result.esp_modem_command_result).c_str());
    }
    delay(200);  // NOLINT
  }
}

bool ModemHandler::prepare_sim() {
  App.feed_wdt();
  delay(200);  // NOLINT
  AtCommandResult result = this->send_at("AT+CPIN?");
  if (!result.success) {
    ESP_LOGW(TAG, "Failed to check pin: %s (%s)", result.c_str(),
             command_result_to_string(result.esp_modem_command_result).c_str());
    return false;
  }

  std::string current_sim_status = result.output.starts_with("+CPIN: ") ? result.output.substr(7) : result.output;
  ESP_LOGD(TAG, "SIM: %s", current_sim_status.c_str());

  if (current_sim_status.find("READY") != std::string::npos) {
    return true;
  }

  if (this->pin_code.empty()) {
    ESP_LOGE(TAG, "SIM PIN empty, but SIM locked.");
    return false;
  }

  ESP_LOGD(TAG, "Unlocking SIM with PIN...");
  if (this->dce->set_pin(this->pin_code) == command_result::OK) {
    ESP_LOGD(TAG, "PIN set. (But not yet checked).");
    return true;
  }

  ESP_LOGE(TAG, "Failed to set PIN.");
  return false;
}

void ModemHandler::ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  auto *handler = static_cast<ModemHandler *>(arg);

  switch (event_id) {
    case IP_EVENT_PPP_GOT_IP: {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
      ESP_LOGD(TAG, "IP event: Got IP " IPSTR, IP2STR(&event->ip_info.ip));
      handler->network_infos.ip_info = event->ip_info;
      esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_MAIN, &handler->network_infos.dns_main);
      esp_netif_get_dns_info(event->esp_netif, ESP_NETIF_DNS_BACKUP, &handler->network_infos.dns_backup);
      handler->network_infos.got_ip = true;
      break;
    }
    case IP_EVENT_PPP_LOST_IP:
      // if (self->component_state_ == ModemComponentState::CONNECTED) {
      //   // Only log if previously connected.
      //   ESP_LOGD(TAG, "IP event: Lost IP.");
      // }
      handler->network_infos.got_ip = false;
      break;
  }
}

}  // namespace modem
}  // namespace esphome
#endif

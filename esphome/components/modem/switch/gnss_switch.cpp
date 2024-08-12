#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_SWITCH

#include "gnns_switch.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "../modem_component.h"

#define ESPHL_ERROR_CHECK(err, message) \
  if ((err) != ESP_OK) { \
    ESP_LOGE(TAG, message ": (%d) %s", err, esp_err_to_name(err)); \
    this->mark_failed(); \
    return; \
  }

#define ESPMODEM_ERROR_CHECK(err, message) \
  if ((err) != command_result::OK) { \
    ESP_LOGE(TAG, message ": %s", command_result_to_string(err).c_str()); \
  }

namespace esphome {
namespace modem {

using namespace esp_modem;

static const char *const TAG = "modem.switch";

optional<bool> GnssSwitch::get_modem_gnss_state() {
  optional<bool> gnss_state;
  auto at_command_result = global_modem_component->send_at(this->command_ + "?");
  if (at_command_result) {
    std::string modem_state = at_command_result.result;
    std::string delimiter = ": ";
    std::size_t pos = modem_state.find(delimiter);
    if (pos != std::string::npos) {
      pos += delimiter.length();
      if (modem_state[pos] == '1') {
        return true;
      } else if (modem_state[pos] == '0') {
        return false;
      }
    }
  }
  return gnss_state;
}

void GnssSwitch::dump_config() { LOG_SWITCH("", "Modem GNSS Switch", this); }

void GnssSwitch::setup() { this->state = this->get_initial_state_with_restore_mode().value_or(false); }

void GnssSwitch::loop() {
  if (!this->modem_state_.has_value()) {
    this->modem_state_ = this->get_modem_gnss_state();
  } else {
    if ((this->state != this->modem_state_)) {
      if (global_modem_component->send_at(this->command_ + (this->state ? "=1" : "=0"))) {
        this->modem_state_ = this->state;
        this->publish_state(this->state);
      }
    }
  }
}

void GnssSwitch::write_state(bool state) { this->state = state; }

}  // namespace modem
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_SWITCH
#endif  // USE_ESP_IDF

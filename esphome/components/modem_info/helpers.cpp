#ifdef USE_ESP32

#include "modem_component.h"
#include "helpers.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <esp_idf_version.h>
#include <esp_task_wdt.h>

#include <unordered_map>
#include <string>

namespace esphome {
namespace modem {

std::string command_result_to_string(command_result err) {
  std::string res = "UNKNOWN";
  switch (err) {
    case command_result::FAIL:
      res = "FAIL";
      break;
    case command_result::OK:
      res = "OK";
      break;
    case command_result::TIMEOUT:
      res = "TIMEOUT";
  }
  return res;
}

std::string modem_mode_to_string(modem_mode mode) {
  std::string res = "UNKNOWN";
  switch (mode) {
    case modem_mode::AUTODETECT:
      res = "AUTODETECT";
      break;
    case modem_mode::COMMAND_MODE:
      res = "COMMAND_MODE";
      break;
    case modem_mode::DATA_MODE:
      res = "DATA_MODE";
      break;
    case modem_mode::DUAL_MODE:
      res = "DUAL_MODE";
      break;
    case modem_mode::CMUX_MODE:
      res = "CMUX_MODE";
      break;
    case modem_mode::CMUX_MANUAL_MODE:
      res = "CMUX_MANUAL_MODE";
      break;
    case modem_mode::CMUX_MANUAL_EXIT:
      res = "CMUX_MANUAL_EXIT";
      break;
    case modem_mode::CMUX_MANUAL_DATA:
      res = "CMUX_MANUAL_DATA";
      break;
    case modem_mode::CMUX_MANUAL_COMMAND:
      res = "CMUX_MANUAL_COMMAND";
      break;
    case modem_mode::CMUX_MANUAL_SWAP:
      res = "CMUX_MANUAL_SWAP";
      break;
    case modem_mode::RESUME_DATA_MODE:
      res = "RESUME_DATA_MODE";
      break;
    case modem_mode::RESUME_COMMAND_MODE:
      res = "RESUME_COMMAND_MODE";
      break;
    case modem_mode::RESUME_CMUX_MANUAL_MODE:
      res = "RESUME_CMUX_MANUAL_MODE";
      break;
    case modem_mode::RESUME_CMUX_MANUAL_DATA:
      res = "RESUME_CMUX_MANUAL_DATA";
      break;
    case modem_mode::UNDEF:
      res = "UNDEF";
      break;
  }
  return res;
}

std::string state_to_string(ModemComponentState state) {
  std::string str;
  switch (state) {
    case ModemComponentState::ENABLING:
      str = "ENABLING";
      break;
    case ModemComponentState::SYNCING:
      str = "SYNCING";
      break;
    case ModemComponentState::DISCONNECTED:
      str = "DISCONNECTED";
      break;
    case ModemComponentState::INIT_NETWORK:
      str = "INIT_NETWORK";
      break;
    case ModemComponentState::START_PPP:
      str = "START_PPP";
      break;
    case ModemComponentState::WAIT_IP:
      str = "WAIT_IP";
      break;
    case ModemComponentState::CONNECTED:
      str = "CONNECTED";
      break;
    case ModemComponentState::NOT_RESPONDING:
      str = "NOT_RESPONDING";
      break;
    case ModemComponentState::DISABLING:
      str = "DISABLING";
      break;
    case ModemComponentState::DISABLED:
      str = "DISABLED";
      break;
  }
  return str;
}

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP32

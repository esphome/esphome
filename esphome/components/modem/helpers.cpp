#ifdef USE_ESP_IDF

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

std::string state_to_string(ModemComponentState state) {
  std::string str;
  switch (state) {
    case ModemComponentState::NOT_RESPONDING:
      str = "NOT_RESPONDING";
      break;
    case ModemComponentState::DISCONNECTED:
      str = "DISCONNECTED";
      break;
    case ModemComponentState::CONNECTED:
      str = "CONNECTED";
      break;
    case ModemComponentState::DISABLED:
      str = "DISABLED";
      break;
  }
  return str;
}

std::string network_system_mode_to_string(int mode) {
  std::string network_type = "Not available";
  // Access Technology from AT+CNSMOD?
  // see https://www.waveshare.com/w/upload/a/af/SIM7500_SIM7600_Series_AT_Command_Manual_V3.00.pdf, page 109
  switch (mode) {
    case 0:
      network_type = "No service";
      break;
    case 1:
      network_type = "GSM";
      break;
    case 2:
      network_type = "GPRS";
      break;
    case 3:
      network_type = "EGPRS (EDGE)";
      break;
    case 4:
      network_type = "WCDMA";
      break;
    case 5:
      network_type = "HSDPA only (WCDMA)";
      break;
    case 6:
      network_type = "HSUPA only (WCDMA)";
      break;
    case 7:
      network_type = "HSPA (HSDPA and HSUPA, WCDMA)";
      break;
    case 8:
      network_type = "LTE";
      break;
    case 9:
      network_type = "TDS-CDMA";
      break;
    case 10:
      network_type = "TDS-HSDPA only";
      break;
    case 11:
      network_type = "TDS-HSUPA only";
      break;
    case 12:
      network_type = "TDS-HSPA (HSDPA and HSUPA)";
      break;
    case 13:
      network_type = "CDMA";
      break;
    case 14:
      network_type = "EVDO";
      break;
    case 15:
      network_type = "HYBRID (CDMA and EVDO)";
      break;
    case 16:
      network_type = "1XLTE (CDMA and LTE)";
      break;
    case 23:
      network_type = "EHRPD";
      break;
    case 24:
      network_type = "HYBRID (CDMA and EHRPD)";
      break;
    default:
      network_type = "Unknown";
  }
  return network_type;
}

std::string get_signal_bars(float rssi) { return get_signal_bars(rssi, true); }
std::string get_signal_bars(float rssi, bool color) {
  // adapted from wifi_component.cpp
  // LOWER ONE QUARTER BLOCK
  // Unicode: U+2582, UTF-8: E2 96 82
  // LOWER HALF BLOCK
  // Unicode: U+2584, UTF-8: E2 96 84
  // LOWER THREE QUARTERS BLOCK
  // Unicode: U+2586, UTF-8: E2 96 86
  // FULL BLOCK
  // Unicode: U+2588, UTF-8: E2 96 88

  if (!color) {
    if (std::isnan(rssi)) {
      return "None";
    } else if (rssi >= -50) {
      return "High";
    } else if (rssi >= -65) {
      return "Good";
    } else if (rssi >= -85) {
      return "Medium";
    } else {
      return "Low";
    }
  } else {
    if (std::isnan(rssi)) {
      return "\033[0;31m"  // red
             "!"
             "\033[0;37m"
             "\xe2\x96\x84"
             "\xe2\x96\x86"
             "\xe2\x96\x88"
             "\033[0m";
    } else if (rssi >= -50) {
      return "\033[0;32m"  // green
             "\xe2\x96\x82"
             "\xe2\x96\x84"
             "\xe2\x96\x86"
             "\xe2\x96\x88"
             "\033[0m";
    } else if (rssi >= -65) {
      return "\033[0;33m"  // yellow
             "\xe2\x96\x82"
             "\xe2\x96\x84"
             "\xe2\x96\x86"
             "\033[0;37m"
             "\xe2\x96\x88"
             "\033[0m";
    } else if (rssi >= -85) {
      return "\033[0;33m"  // yellow
             "\xe2\x96\x82"
             "\xe2\x96\x84"
             "\033[0;37m"
             "\xe2\x96\x86"
             "\xe2\x96\x88"
             "\033[0m";
    } else {
      return "\033[0;31m"  // red
             "\xe2\x96\x82"
             "\033[0;37m"
             "\xe2\x96\x84"
             "\xe2\x96\x86"
             "\xe2\x96\x88"
             "\033[0m";
    }
  }
}

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF

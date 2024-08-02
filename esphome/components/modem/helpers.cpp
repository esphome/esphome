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

// Access Technology from AT+CNSMOD?
// see https://www.waveshare.com/w/upload/a/af/SIM7500_SIM7600_Series_AT_Command_Manual_V3.00.pdf, page 109
static const std::unordered_map<int, std::string> ACT_MAP = {{0, "No service"},
                                                             {1, "GSM"},
                                                             {2, "GPRS"},
                                                             {3, "EGPRS (EDGE)"},
                                                             {4, "WCDMA"},
                                                             {5, "HSDPA only (WCDMA)"},
                                                             {6, "HSUPA only (WCDMA)"},
                                                             {7, "HSPA (HSDPA and HSUPA, WCDMA)"},
                                                             {8, "LTE"},
                                                             {9, "TDS-CDMA"},
                                                             {10, "TDS-HSDPA only"},
                                                             {11, "TDS-HSUPA only"},
                                                             {12, "TDS-HSPA (HSDPA and HSUPA)"},
                                                             {13, "CDMA"},
                                                             {14, "EVDO"},
                                                             {15, "HYBRID (CDMA and EVDO)"},
                                                             {16, "1XLTE (CDMA and LTE)"},
                                                             {23, "EHRPD"},
                                                             {24, "HYBRID (CDMA and EHRPD)"}};

std::string get_network_type_name(int act) { return ACT_MAP.at(act); }

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP_IDF

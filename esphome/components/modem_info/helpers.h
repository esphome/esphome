#pragma once

#ifdef USE_ESP32

#include "modem_component.h"

#include <cxx_include/esp_modem_api.hpp>

namespace esphome {
namespace modem {

std::string command_result_to_string(command_result err);
std::string modem_mode_to_string(modem_mode mode);
std::string state_to_string(ModemComponentState state);
std::string network_system_mode_to_string(int mode);
std::string get_signal_bars(float rssi);
std::string get_signal_bars(float rssi, bool color);

}  // namespace modem
}  // namespace esphome
#endif  // USE_ESP32

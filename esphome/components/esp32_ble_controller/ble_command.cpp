#include "ble_command.h"

#include "esp32_ble_controller.h"
#include "automation.h"

namespace esphome {
namespace esp32_ble_controller {

static const char *TAG = "ble_command";

// generic ///////////////////////////////////////////////////////////////////////////////////////////////

void BLECommand::set_result(const string& result) const {
  global_ble_controller->set_command_result(result);
}

string BLECommand::get_command_specific_help() const {
  return get_description();
}

// help ///////////////////////////////////////////////////////////////////////////////////////////////

BLECommandHelp::BLECommandHelp() : BLECommand("help", "shows help for commands.") {}

void BLECommandHelp::execute(const vector<string>& arguments) const {
  if (arguments.empty()) {
    string help("Availabe:");
    for (const auto& command : global_ble_controller->get_commands()) {
      help += ' ';
      help += command->get_name();
    }
    help += ", 'help <cmd>' for more.";
    set_result(help);
  } else {
    string command_name = arguments[0];
    for (const auto& command : global_ble_controller->get_commands()) {
      if (command->get_name() == command_name) {
        set_result(command_name + ": " + command->get_command_specific_help());
        return;
      }
      set_result("Unknown BLE command '" + command_name + "'");
    }
  }
}

// ble-services ///////////////////////////////////////////////////////////////////////////////////////////////

BLECommandSwitchServicesOnOrOff::BLECommandSwitchServicesOnOrOff() : BLECommand("ble-services", "'ble-services on|off' enables or disables the non-maintenance BLE services.") {}

void BLECommandSwitchServicesOnOrOff::execute(const vector<string>& arguments) const {
  if (!arguments.empty()) {
    const string& on_or_off = arguments[0];
    if (on_or_off == "off") {
      global_ble_controller->set_ble_mode(BLEMaintenanceMode::WIFI_ONLY);
    } else {
      global_ble_controller->set_ble_mode(BLEMaintenanceMode::MIXED);
    }
  }
  BLEMaintenanceMode mode = global_ble_controller->get_ble_mode();
  string enabled_or_disabled = mode == BLEMaintenanceMode::WIFI_ONLY ? "disabled" : "enabled";
  set_result("Non-maintenance services are " + enabled_or_disabled +".");
}

// wifi-config ///////////////////////////////////////////////////////////////////////////////////////////////

BLECommandWifiConfiguration::BLECommandWifiConfiguration() : BLECommand("wifi-config", "sets or clears the WIFI configuration") {}

void BLECommandWifiConfiguration::execute(const vector<string>& arguments) const {
  if (arguments.size() >= 2 && arguments.size() <= 3) {
    const string& ssid = arguments[0];
    const string& password = arguments[1];
    const bool hidden_network = arguments.size() == 3 && arguments[2] == "hidden";
    global_ble_controller->set_wifi_configuration(ssid, password, hidden_network);
    set_result("WIFI configuration updated.");
  } else if (arguments.size() == 1 && arguments[0] == "clear") {
    set_result("WIFI configuration cleared.");
    global_ble_controller->clear_wifi_configuration_and_reboot();
  } else if (arguments.empty()) {
    auto ssid = global_ble_controller->get_current_ssid_in_wifi_configuration();
    if (ssid.has_value()) {
      set_result("WIFI configuration present for network " + ssid.value() + ".");
    } else {
      set_result("WIFI configuration not overridden, default used.");
    }
  } else {
    set_result("Invalid arguments!");
  }
}

string BLECommandWifiConfiguration::get_command_specific_help() const {
    auto ssid = global_ble_controller->get_current_ssid_in_wifi_configuration();
    if (ssid.has_value()) {
      return "'wifi-config clear' clears the configuration and reverts to default WIFI configuration.";
    } else {
      return "'wifi-config <ssid> <pwd> [hidden]' sets WIFI SSID and password and if the network is hidden.";
    }
}

// log-level ///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef USE_LOGGER
BLECommandLogLevel::BLECommandLogLevel() : BLECommand("log-level", "gets or sets log level (0=None, 4=Config, 5=Debug).") {}

void BLECommandLogLevel::execute(const vector<string>& arguments) const {
  if (!arguments.empty()) {
    string log_level = arguments[0];
    const optional<int> level = parse_int(log_level);
    if (level.has_value()) {
      global_ble_controller->set_log_level(level.value());
    }
  }

  set_result("Log level is " + to_string(global_ble_controller->get_log_level())+".");
}
#endif

// custom ///////////////////////////////////////////////////////////////////////////////////////////////

BLECustomCommand::BLECustomCommand(const string& name, const string& description, BLEControllerCustomCommandExecutionTrigger* trigger)
 : BLECommand(name, description), trigger(trigger) {}

void BLECustomCommand::execute(const vector<string>& arguments) const {
  BLECustomCommandResultHolder result_holder;

  trigger->trigger(arguments, result_holder);

  optional<string> result = result_holder.get_result();
  if (result.has_value()) {
    ESP_LOGD(TAG, "Setting result for custom command %s: %s", get_name().c_str(), result.value().c_str());
    set_result(result.value());
  }
}

} // namespace esp32_ble_controller
} // namespace esphome

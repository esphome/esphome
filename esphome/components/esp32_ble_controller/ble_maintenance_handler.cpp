#include <BLE2902.h>

#include "ble_maintenance_handler.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include "esp32_ble_controller.h"
#include "ble_command.h"
#include "automation.h"
#include "ble_utils.h"

// https://www.uuidgenerator.net
#define SERVICE_UUID                "7b691dff-9062-4192-b46a-692e0da81d91"
#define CHARACTERISTIC_UUID_CMD     "1d3c6498-cfdf-44a1-9038-3e757dcc449d"
#define CHARACTERISTIC_UUID_LOGGING "a1083f3b-0ad6-49e0-8a9d-56eb5bf462ca"

namespace esphome {
namespace esp32_ble_controller {

static const char *TAG = "ble_maintenance_handler";

BLEMaintenanceHandler::BLEMaintenanceHandler() {
  commands.push_back(new BLECommandHelp());
  commands.push_back(new BLECommandSwitchServicesOnOrOff());
  commands.push_back(new BLECommandWifiConfiguration());

#ifdef USE_LOGGER
  commands.push_back(new BLECommandLogLevel());
#endif
}

void BLEMaintenanceHandler::setup(BLEServer* ble_server) {
  ESP_LOGCONFIG(TAG, "Setting up maintenance service");

  BLEService* service = ble_server->createService(SERVICE_UUID);

  ble_command_characteristic = create_writeable_ble_characteristic(service, CHARACTERISTIC_UUID_CMD, this, "BLE Command Channel");
  ble_command_characteristic->setValue("Send 'help' for help.");
 
#ifdef USE_LOGGER
  logging_characteristic = create_read_only_ble_characteristic(service, CHARACTERISTIC_UUID_LOGGING, "Log messages");
#endif

  service->start();

#ifdef USE_LOGGER
  log_level = ESPHOME_LOG_LEVEL;
  if (global_ble_controller->get_ble_mode() != BLEMaintenanceMode::BLE_ONLY) {
    log_level = ESPHOME_LOG_LEVEL_CONFIG;
  }

  // NOTE: We register the callback after the service has been started!
  if (logger::global_logger != nullptr) {
    logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
      // publish log message
      this->send_log_message(level, tag, message);
    });
  }
#endif
}

void BLEMaintenanceHandler::onWrite(BLECharacteristic *characteristic) {
  if (characteristic == ble_command_characteristic) {
    global_ble_controller->execute_in_loop([this](){ on_command_written(); });
  } else {
    ESP_LOGW(TAG, "Unknown characteristic written!");
  }
}

void BLEMaintenanceHandler::on_command_written() {
  string command_line = ble_command_characteristic->getValue();
  ESP_LOGD(TAG, "Received BLE command: %s", command_line.c_str());
  vector<string> tokens = split(command_line);
  if (!tokens.empty()) {
    string command_name = tokens[0];
    for (const auto& command : get_commands()) {
      if (command->get_name() == command_name) {
        ESP_LOGI(TAG, "Executing BLE command: %s", command_name.c_str());
        tokens.erase(tokens.begin());
        command->execute(tokens);
        return;
      }
    }
    set_command_result("Unkown command '" + command_name + "', try 'help'.");
  }
}

void BLEMaintenanceHandler::set_command_result(const string& result_message) {
  global_ble_controller->execute_in_loop([this, result_message] { 
     ble_command_characteristic->setValue(result_message);
  });
  // global_ble_controller->execute_in_loop([this, result_message] { 
  //   const uint32_t delay_millis = 50;
  //   App.scheduler.set_timeout(global_ble_controller, "command_result", delay_millis, [this, result_message]{ ble_command_characteristic->setValue(result_message); });
  // });
}

bool BLEMaintenanceHandler::is_security_enabled() {
  return global_ble_controller->get_security_enabled();
}

#ifdef USE_LOGGER
/**
 * Removes magic logger symbols from the message, e.g., sequences that mark the start or the end, or a color.
 */
string remove_logger_magic(const string& message) {
  // Note: We do not use regex replacement because it enlarges the binary by roughly 50kb!
  string result;
  boolean within_magic = false;
  for (string::size_type i = 0; i < message.length() - 1; ++i) {
    if (message[i] == '\033' && message[i+1] == '[') { // log magic always starts with "\033[" see log.h
      within_magic = true;
      ++i;
    } else if (within_magic) {
      within_magic = (message[i] != 'm');
    } else {
      result.push_back(message[i]);
    }
  }
  return result;
}

void BLEMaintenanceHandler::send_log_message(int level, const char *tag, const char *message) {
  if (level <= this->log_level) {
    logging_characteristic->setValue(remove_logger_magic(message));
    logging_characteristic->notify();
  }
}
#endif

} // namespace esp32_ble_controller
} // namespace esphome

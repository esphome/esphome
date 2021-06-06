#pragma once

#include <string>
#include <vector>

#include <BLEServer.h>
#include <BLECharacteristic.h>

#include "esphome/core/defines.h"

using std::string;
using std::vector;

namespace esphome {
namespace esp32_ble_controller {

enum class BLEMaintenanceMode : uint8_t { BLE_ONLY, MIXED, WIFI_ONLY };

class BLECommand;
class BLEControllerCustomCommandExecutionTrigger;

/**
 * Provides standard maintenance support for the BLE controller like logging over BLE and controlling BLE mode.
 * It does not control individual ESPHome components (like sensors, switches, ...), but rather provides generic global functionality.
 * It provides a special BLE service with its own characteristics.
 * @brief Provides maintenance support for BLE clients (like controlling the BLE mode and logging over BLE)
 */
class BLEMaintenanceHandler : private BLECharacteristicCallbacks {
public:
  BLEMaintenanceHandler();
  virtual ~BLEMaintenanceHandler() {}

  void setup(BLEServer* ble_server);

  void add_command(BLECommand* command) { commands.push_back(command); }
  const vector<BLECommand*>& get_commands() const { return commands; }
  void set_command_result(const string& result_message);

  void set_ble_mode(BLEMaintenanceMode mode);

#ifdef USE_LOGGER
  int get_log_level() { return log_level; }
  void set_log_level(int level) { log_level = level; }

  void send_log_message(int level, const char *tag, const char *message);
#endif

private:
  virtual void onWrite(BLECharacteristic *characteristic) override;
  void on_command_written();

  bool is_security_enabled();
  
private:
  BLEService* maintenance_service;

  BLECharacteristic* ble_command_characteristic;
  vector<BLECommand*> commands;

#ifdef USE_LOGGER
  int log_level;

  BLECharacteristic* logging_characteristic;
#endif
};

} // namespace esp32_ble_controller
} // namespace esphome

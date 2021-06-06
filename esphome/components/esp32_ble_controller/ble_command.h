#pragma once

#include <string>
#include <vector>

#include "esphome/core/defines.h"

using std::string;
using std::vector;

namespace esphome {
namespace esp32_ble_controller {

// generic ///////////////////////////////////////////////////////////////////////////////////////////////

class BLECommand {
public:
  BLECommand(const string& name, const string& description) : name(name), description(description) {}
  virtual ~BLECommand() {}

  const string get_name() const { return name; }
  const string get_description() const { return description; }

  virtual void execute(const std::vector<string>& arguments) const = 0;

  virtual string get_command_specific_help() const;
  
protected:
  void set_result(const string& result) const;

private:
  string name;
  string description;
};

// help ///////////////////////////////////////////////////////////////////////////////////////////////

class BLECommandHelp : public BLECommand {
public:
  BLECommandHelp();
  virtual ~BLECommandHelp() {}

  virtual void execute(const vector<string>& arguments) const override;
};

// ble-services ///////////////////////////////////////////////////////////////////////////////////////////////

class BLECommandSwitchServicesOnOrOff : public BLECommand {
public:
  BLECommandSwitchServicesOnOrOff();
  virtual ~BLECommandSwitchServicesOnOrOff() {}

  virtual void execute(const vector<string>& arguments) const override;
};

// wifi-config ///////////////////////////////////////////////////////////////////////////////////////////////

class BLECommandWifiConfiguration : public BLECommand {
public:
  BLECommandWifiConfiguration();
  virtual ~BLECommandWifiConfiguration() {}

  virtual void execute(const vector<string>& arguments) const override;

  virtual string get_command_specific_help() const override;
};

// log-level ///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef USE_LOGGER
class BLECommandLogLevel : public BLECommand {
public:
  BLECommandLogLevel();
  virtual ~BLECommandLogLevel() {}

  virtual void execute(const vector<string>& arguments) const override;
};
#endif

// custom ///////////////////////////////////////////////////////////////////////////////////////////////

class BLEControllerCustomCommandExecutionTrigger;

class BLECustomCommand : public BLECommand {
public:
  BLECustomCommand(const string& name, const string& description, BLEControllerCustomCommandExecutionTrigger* trigger);
  virtual ~BLECustomCommand() {}

  virtual void execute(const vector<string>& arguments) const override;

private:
  BLEControllerCustomCommandExecutionTrigger* trigger;
};

} // namespace esp32_ble_controller
} // namespace esphome

#pragma once

#include <string>
#include <list>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#include "dbus-wrapper.h"
#include "VariantTree.h"

namespace esphome {
namespace dbus {

class DBus : public Component, public DBusWrapper {
 public:
  DBus() : system_dbus(true){};
  ~DBus();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  //  std::string unique_id() override;

  // void loop() override;
  //   void update() override; -> pollingComponent

  // Actions
  void send(const optional<bool> &dbus_system, const optional<std::string> &dbus_destination,
            const optional<std::string> &dbus_path, const optional<std::string> &dbus_interface,
            const optional<std::string> &dbus_method, const optional<VariantTree > &dbus_args);

 protected:
 private:
  DBusWrapper user_dbus;
  DBusWrapper system_dbus;
  bool user_dbus_initialized = false;
  bool system_dbus_initialized = false;
};

}  // namespace dbus
}  // namespace esphome

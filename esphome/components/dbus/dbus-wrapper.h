#pragma once

#ifdef USE_HOST

#include <string>
#include <list>
#include <dbus/dbus.h>

#include "esphome/core/defines.h"
#include "VariantTree.h"

namespace esphome {
namespace dbus {

std::string dbus_iter_to_string(DBusMessageIter *iter);

class DBusWrapper {
 public:
  DBusWrapper(bool dbus_system = false) : system_(dbus_system){};
  ~DBusWrapper();
  void setup();

  std::string send(const std::string &dbus_destination, const std::string &dbus_path, const std::string &dbus_interface,
                   const std::string &dbus_method, const VariantTree &dbus_args,
                   const std::list<std::string> &properties = {}, const std::string &property_separator = "");

  std::string get_property(DBusMessage *msg, const std::string &search_key);

  void set_system(bool system) { this->system_ = system; };

 protected:
  DBusConnection *conn_{NULL};
  void register_for_signal_(const std::string &dbus_properties, const std::string &dbus_path);
  bool system_ = false;
};

}  // namespace dbus
}  // namespace esphome

#endif

#pragma once

#include <string.h>
#include <list>
#include "VariantTree.h"

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "dbus-wrapper.h"

namespace esphome {
namespace dbus {

class DBusTextSensor : public text_sensor::TextSensor, public PollingComponent, public DBusWrapper {
 public:
  DBusTextSensor()
      : dbus_destination(),
        dbus_path(),
        dbus_interface(),
        dbus_method(),
        dbus_args({}),
        properties(),
        property_separator(){};
  ~DBusTextSensor();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  // std::string unique_id() override;

  void update();

  void set_dbus_destination(const std::string &dbus_destination) { this->dbus_destination = dbus_destination; };
  void set_dbus_path(const std::string &dbus_path) { this->dbus_path = dbus_path; };
  void set_dbus_interface(const std::string &dbus_interface) { this->dbus_interface = dbus_interface; };
  void set_dbus_method(const std::string &dbus_method) { this->dbus_method = dbus_method; };
  void set_dbus_args(const VariantTree &dbus_args) { this->dbus_args = dbus_args; };
  void set_properties(const std::list<std::string> &properties) { this->properties = properties; };
  void set_property_separator(const std::string &property_separator) { this->property_separator = property_separator; };

 protected:
  std::string dbus_destination;
  std::string dbus_path;
  std::string dbus_interface;
  std::string dbus_method;
  VariantTree dbus_args;
  std::list<std::string> properties;
  std::string property_separator;
  void loop() override;
};

}  // namespace dbus
}  // namespace esphome

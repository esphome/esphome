#pragma once
#ifdef USE_HOST

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
      : dbus_destination_(),
        dbus_path_(),
        dbus_interface_(),
        dbus_method_(),
        dbus_args_({}),
        properties_(),
        property_separator_(){};
  ~DBusTextSensor();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  // std::string unique_id() override;

  void update() override;

  void set_dbus_destination(const std::string &dbus_destination) { this->dbus_destination_ = dbus_destination; };
  void set_dbus_path(const std::string &dbus_path) { this->dbus_path_ = dbus_path; };
  void set_dbus_interface(const std::string &dbus_interface) { this->dbus_interface_ = dbus_interface; };
  void set_dbus_method(const std::string &dbus_method) { this->dbus_method_ = dbus_method; };
  void set_dbus_args(const VariantTree &dbus_args) { this->dbus_args_ = dbus_args; };
  void set_properties(const std::list<std::string> &properties) { this->properties_ = properties; };
  void set_property_separator(const std::string &property_separator) {
    this->property_separator_ = property_separator;
  };

 protected:
  std::string dbus_destination_;
  std::string dbus_path_;
  std::string dbus_interface_;
  std::string dbus_method_;
  VariantTree dbus_args_;
  std::list<std::string> properties_;
  std::string property_separator_;
  void loop() override;
};

}  // namespace dbus
}  // namespace esphome
#endif

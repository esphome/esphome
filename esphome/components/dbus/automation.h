#pragma once
#ifdef USE_HOST

#include "esphome/core/automation.h"
#include "dbus.h"
#include <variant>
#include <vector>

#include "VariantTree.h"

namespace esphome {
namespace dbus {

template<typename... Ts> class DBusSendAction : public Action<Ts...>, public Parented<DBus> {
 public:
  TEMPLATABLE_VALUE(bool, dbus_system)
  TEMPLATABLE_VALUE(std::string, dbus_destination)
  TEMPLATABLE_VALUE(std::string, dbus_path)
  TEMPLATABLE_VALUE(std::string, dbus_interface)
  TEMPLATABLE_VALUE(std::string, dbus_method)
  // std::list<std::string> requires extra set_* functions
  TEMPLATABLE_VALUE(VariantTree, dbus_args)
  void set_dbus_args(const VariantTree &dbus_args) { this->dbus_args_ = dbus_args; }

  void play(const Ts &...x) override {
    this->parent_->send(this->dbus_system_.optional_value(x...), this->dbus_destination_.optional_value(x...),
                        this->dbus_path_.optional_value(x...), this->dbus_interface_.optional_value(x...),
                        this->dbus_method_.optional_value(x...), this->dbus_args_.optional_value(x...));
  }
};

}  // namespace dbus
}  // namespace esphome
#endif

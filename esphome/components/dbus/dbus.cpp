#ifdef USE_HOST

#include "dbus.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include "dbus-print-message.h"

namespace esphome {
namespace dbus {

static const char *const TAG = "dbus";

void VariantTree::print() const { std::visit(VariantTreePrinter{}, this->data); }

DBus::~DBus() {}

void DBus::setup() {
  printf("[%s] DBus::setup()\n", "DBUS");
  ESP_LOGV(TAG, "%s  DBus::setup(): '%s'", prefix, (obj)->unique_id().c_str());
}

/**
 * dbus.send action
 */
void DBus::send(const optional<bool> &dbus_system, const optional<std::string> &dbus_destination,
                const optional<std::string> &dbus_path, const optional<std::string> &dbus_interface,
                const optional<std::string> &dbus_method, const optional<VariantTree> &dbus_args) {
  printf("DBus::send ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  if (!dbus_destination.has_value()) {
    printf("no dbus_destination\n");
    return;
  }
  if (!dbus_path.has_value()) {
    printf("no dbus_path\n");
    return;
  }
  if (!dbus_interface.has_value()) {
    printf("no dbus_interface\n");
    return;
  }
  if (!dbus_method.has_value()) {
    printf("no dbus_method\n");
    return;
  }
  if (!dbus_args.has_value()) {
    printf("no dbus_args\n");
    return;
  }
  std::list<std::string> dbus_args_list;

  if (dbus_system.has_value() && dbus_system.value()) {
    if (!this->system_dbus_initialized_) {
      this->system_dbus_.setup();
      this->system_dbus_initialized_ = true;
      printf("[%s] DBus::system_dbus_::setup()\n", "DBUS");
    }
    this->system_dbus_.send(dbus_destination.value(), dbus_path.value(), dbus_interface.value(), dbus_method.value(),
                            dbus_args.value());
  } else {
    if (!this->user_dbus_initialized_) {
      this->user_dbus_.setup();
      this->user_dbus_initialized_ = true;
      printf("[%s] DBus::user_dbus_::setup()\n", "DBUS");
    }

    this->user_dbus_.send(dbus_destination.value(), dbus_path.value(), dbus_interface.value(), dbus_method.value(),
                          dbus_args.value());
  }
}

float DBus::get_setup_priority() const { return setup_priority::DATA; }
void DBus::dump_config() {
  // dbus component doesn't have any configuration
  ESP_LOGCONFIG(TAG, "dbus", this);
}

/*
void DBus::loop() {
  printf("[%s] DBus::loop() done\n", "DBUS");
}
*/

}  // namespace dbus
}  // namespace esphome
#endif

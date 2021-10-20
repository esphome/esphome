#include "esphome/core/device_registry.h"
#include "esphome/core/helpers.h"

namespace esphome {

static const char *const TAG = "device_registry";

// Device Name
const std::string &DeviceRegistry::get_name() const { return this->name_; }
void DeviceRegistry::set_name(const std::string &name) { this->name_ = name; }

// Identifiers
const std::vector<std::string> &DeviceRegistry::get_identifiers() const { return this->identifiers_; }
void DeviceRegistry::add_identifier(const std::string identifier) { this->identifiers_.push_back(identifier); }

// Connections
const std::vector<std::tuple<std::string, std::string>> &DeviceRegistry::get_connections() const {
  return this->connections_;
}
void DeviceRegistry::add_connection(const std::string connection_type, const std::string connection_identifier) {
  this->connections_.push_back(std::make_tuple(connection_type, connection_identifier));
}

// Device Manufacturer
const std::string &DeviceRegistry::get_manufacturer() const { return this->manufacturer_; }
void DeviceRegistry::set_manufacturer(const std::string &manufacturer) { this->manufacturer_ = manufacturer; }

// Device Model
const std::string &DeviceRegistry::get_model() const { return this->model_; }
void DeviceRegistry::set_model(const std::string &model) { this->model_ = model; }

// Suggested Area
const std::string &DeviceRegistry::get_suggested_area() const { return this->suggested_area_; }
void DeviceRegistry::set_suggested_area(const std::string &suggested_area) { this->suggested_area_ = suggested_area; }

// Software Version
const std::string &DeviceRegistry::get_software_version() const { return this->software_version_; }
void DeviceRegistry::set_software_version(const std::string &software_version) {
  this->software_version_ = software_version;
}

// Via Device
const std::string &DeviceRegistry::get_via_device() const { return this->via_device_; }
void DeviceRegistry::set_via_device(const std::string &via_device) { this->via_device_ = via_device; }

}  // namespace esphome

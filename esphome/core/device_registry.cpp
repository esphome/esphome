#include "esphome/core/device_registry.h"
#include "esphome/core/helpers.h"

namespace esphome {

static const char *const TAG = "device_registry";

// Device Name
const std::string &DeviceRegistryEntry::get_name() const { return this->name_; }
void DeviceRegistryEntry::set_name(const std::string &name) { this->name_ = name; }

// Identifiers
const std::vector<std::string> &DeviceRegistryEntry::get_identifiers() const { return this->identifiers_; }
void DeviceRegistryEntry::add_identifier(const std::string &identifier) { this->identifiers_.push_back(identifier); }

// Device Manufacturer
const std::string &DeviceRegistryEntry::get_manufacturer() const { return this->manufacturer_; }
void DeviceRegistryEntry::set_manufacturer(const std::string &manufacturer) { this->manufacturer_ = manufacturer; }

// Device Model
const std::string &DeviceRegistryEntry::get_model() const { return this->model_; }
void DeviceRegistryEntry::set_model(const std::string &model) { this->model_ = model; }

// Suggested Area
const std::string &DeviceRegistryEntry::get_suggested_area() const { return this->suggested_area_; }
void DeviceRegistryEntry::set_suggested_area(const std::string &suggested_area) {
  this->suggested_area_ = suggested_area;
}

// Configuration URL
const std::string &DeviceRegistryEntry::get_configuration_url() const { return this->configuration_url_; }
void DeviceRegistryEntry::set_configuration_url(const std::string &configuration_url) {
  this->configuration_url_ = configuration_url;
}

// Software Version
const std::string &DeviceRegistryEntry::get_software_version() const { return this->software_version_; }
void DeviceRegistryEntry::set_software_version(const std::string &software_version) {
  this->software_version_ = software_version;
}

// Via Device
const std::string &DeviceRegistryEntry::get_via_device() const { return this->via_device_; }
void DeviceRegistryEntry::set_via_device(const std::string &via_device) { this->via_device_ = via_device; }

}  // namespace esphome

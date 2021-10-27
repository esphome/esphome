#pragma once

#include <string>
#include <vector>

namespace esphome {

// An entry in the Device Registry.
//
// An entry in the device registry describes the hardware and software of a node,
// including metadata such as name, manufacturer, and model.
//
// Further Info: https://developers.home-assistant.io/docs/device_registry_index
class DeviceRegistryEntry {
 public:
  // The name of this Device
  const std::string &get_name() const;
  void set_name(const std::string &name);

  // Unique Identifiers
  const std::vector<std::string> &get_identifiers() const;
  void add_identifier(const std::string &identifier);

  // Connections
  const std::vector<std::tuple<std::string, std::string>> &get_connections() const;
  void add_connection(const std::string &connection_type, const std::string &connection_identifier);

  // Device Manufacturer
  const std::string &get_manufacturer() const;
  void set_manufacturer(const std::string &manufacturer);

  // Device Model
  const std::string &get_model() const;
  void set_model(const std::string &model);

  // Suggested Area
  const std::string &get_suggested_area() const;
  void set_suggested_area(const std::string &suggested_area);

  // Software Version
  const std::string &get_software_version() const;
  void set_software_version(const std::string &software_version);

  // Via Device
  const std::string &get_via_device() const;
  void set_via_device(const std::string &via_device);

 protected:
  std::string name_;
  std::vector<std::tuple<std::string, std::string>> connections_;
  std::vector<std::string> identifiers_;
  std::string manufacturer_;
  std::string model_;
  std::string suggested_area_;
  std::string software_version_;
  std::string via_device_;
};

}  // namespace esphome

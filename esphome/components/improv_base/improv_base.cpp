#include "improv_base.h"

#include "esphome/components/network/util.h"
#include "esphome/core/application.h"

namespace esphome {
namespace improv_base {

std::string ImprovBase::get_formatted_next_url_() {
  if (this->next_url_.empty()) {
    return "";
  }

  std::string formatted_url = this->next_url_;

  // Replace all occurrences of {{device_name}}
  const std::string device_name_placeholder = "{{device_name}}";
  const std::string &device_name = App.get_name();
  size_t pos = 0;
  while ((pos = formatted_url.find(device_name_placeholder, pos)) != std::string::npos) {
    formatted_url.replace(pos, device_name_placeholder.length(), device_name);
    pos += device_name.length();
  }

  // Replace all occurrences of {{ip_address}}
  const std::string ip_address_placeholder = "{{ip_address}}";
  std::string ip_address_str;
  for (auto &ip : network::get_ip_addresses()) {
    if (ip.is_ip4()) {
      ip_address_str = ip.str();
      break;
    }
  }
  pos = 0;
  while ((pos = formatted_url.find(ip_address_placeholder, pos)) != std::string::npos) {
    formatted_url.replace(pos, ip_address_placeholder.length(), ip_address_str);
    pos += ip_address_str.length();
  }

  // Note: {{esphome_version}} is replaced at code generation time in Python

  return formatted_url;
}

}  // namespace improv_base
}  // namespace esphome

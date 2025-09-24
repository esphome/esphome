#include "improv_base.h"

#include "esphome/components/network/util.h"
#include "esphome/core/application.h"

namespace esphome {
namespace improv_base {

std::string ImprovBase::get_formatted_next_url_() {
  if (this->next_url_.empty()) {
    return "";
  }
  std::string copy = this->next_url_;
  // Device name
  std::size_t pos = this->next_url_.find("{{device_name}}");
  if (pos != std::string::npos) {
    const std::string &device_name = App.get_name();
    copy.replace(pos, 15, device_name);
  }

  // Ip address
  pos = this->next_url_.find("{{ip_address}}");
  if (pos != std::string::npos) {
    for (auto &ip : network::get_ip_addresses()) {
      if (ip.is_ip4()) {
        std::string ipa = ip.str();
        copy.replace(pos, 14, ipa);
        break;
      }
    }
  }

  return copy;
}

}  // namespace improv_base
}  // namespace esphome

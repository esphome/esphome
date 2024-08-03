#include "esphome/core/defines.h"
#if defined(USE_HOST) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

namespace esphome {
namespace mdns {

void MDNSComponent::setup() { this->compile_records_(); }

void MDNSComponent::on_shutdown() {}
std::vector<network::IPAddress> MDNSComponent::resolve(const std::string &) {
  return std::vector<network::IPAddress>{};
}

MDNSComponent *global_mdns = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace mdns
}  // namespace esphome

#endif

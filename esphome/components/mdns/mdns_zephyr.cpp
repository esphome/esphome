#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_MDNS)

#include "mdns_component.h"

namespace esphome::mdns {

#ifdef USE_MDNS_STORE_SERVICES
// Zephyr has no local IP mDNS responder. When a consumer (ex. the OpenThread SRP
// client) enables service storage, it reads the compiled records via get_services()
// and advertises them itself. We only need to compile and store the records here.
static void register_zephyr(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &) {}

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_zephyr); }
#else
// No responder and nothing consuming the records, so skip the boot-time compile.
void MDNSComponent::setup() {}
#endif

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR && USE_MDNS

#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_MDNS)

#include "mdns_component.h"
#include "esphome/core/log.h"

namespace esphome::mdns {

static const char *const TAG = "mdns.zephyr";

#ifdef USE_MDNS_STORE_SERVICES
// Zephyr has no local IP mDNS responder. When a consumer (ex. the OpenThread SRP
// client) enables service storage, it reads the compiled records via get_services()
// and advertises them itself. We only need to compile and store the records here.
static void register_zephyr(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &) {}

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_zephyr); }
#else

// Nothing on Zephyr advertises mDNS or consumes the compiled records without a
// consumer such as OpenThread SRP, so don't waste the boot-time compile
void MDNSComponent::setup() { ESP_LOGW(TAG, "mDNS is not implemented on Zephyr"); }
#endif

void MDNSComponent::on_shutdown() {}

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR && USE_MDNS

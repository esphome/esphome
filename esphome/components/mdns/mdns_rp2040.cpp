#include "esphome/core/defines.h"
#if defined(USE_RP2040) && defined(USE_MDNS)

#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "mdns_component.h"

// Arduino-Pico's PolledTimeout.h (pulled in by ESP8266mDNS.h) redefines IRAM_ATTR to empty.
// Save and restore our definition around the include to avoid a redefinition warning.
#pragma push_macro("IRAM_ATTR")
#undef IRAM_ATTR
#include <ESP8266mDNS.h>
#pragma pop_macro("IRAM_ATTR")

namespace esphome::mdns {

static void register_rp2040(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  MDNS.begin(App.get_name().c_str());

  for (const auto &service : services) {
    // Strip the leading underscore from the proto and service_type. While it is
    // part of the wire protocol to have an underscore, and for example ESP-IDF
    // expects the underscore to be there, the ESP8266 implementation always adds
    // the underscore itself.
    auto *proto = MDNS_STR_ARG(service.proto);
    while (*proto == '_') {
      proto++;
    }
    auto *service_type = MDNS_STR_ARG(service.service_type);
    while (*service_type == '_') {
      service_type++;
    }
    uint16_t port = const_cast<TemplatableValue<uint16_t> &>(service.port).value();
    MDNS.addService(service_type, proto, port);
    for (const auto &record : service.txt_records) {
      MDNS.addServiceTxt(service_type, proto, MDNS_STR_ARG(record.key), MDNS_STR_ARG(record.value));
    }
  }
}

void MDNSComponent::setup() {
  // RP2040's LEAmDNS library registers a LwipIntf::stateUpCB() callback to restart
  // mDNS when the network interface reconnects. However, stateUpCB() is stubbed out
  // in arduino-pico's LwipIntfCB.cpp because the original ESP8266 implementation used
  // schedule_function() which doesn't exist in arduino-pico, and the callback can't
  // safely run directly since netif status callbacks fire from IRQ context
  // (PICO_CYW43_ARCH_THREADSAFE_BACKGROUND) while _restart() allocates UDP sockets.
  //
  // Workaround: defer MDNS.begin() and service registration until the network is
  // connected (has an IP), then call notifyAPChange() on subsequent reconnects to
  // restart mDNS probing and announcing — all from main loop context so it's
  // thread-safe.
  this->set_interval(MDNS_UPDATE_INTERVAL_MS, [this]() {
    bool connected = network::is_connected();
    if (connected && !this->was_connected_) {
      if (!this->initialized_) {
        this->setup_buffers_and_register_(register_rp2040);
        this->initialized_ = true;
      } else {
        MDNS.notifyAPChange();
      }
    }
    this->was_connected_ = connected;
    if (this->initialized_) {
      MDNS.update();
    }
  });
}

void MDNSComponent::on_shutdown() {
  MDNS.close();
  delay(40);
}

}  // namespace esphome::mdns

#endif

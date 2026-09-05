#include "esphome/core/defines.h"
#if defined(USE_ESP8266) && defined(USE_ARDUINO) && defined(USE_MDNS)

#include <ESP8266mDNS.h>
#include "esphome/components/network/ip_address.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "mdns_component.h"
// wifi_component.h is pulled in transitively by mdns_component.h when
// USE_MDNS_WIFI_LISTENER is defined.

namespace esphome::mdns {

// Main-loop calls into LEAmDNS (update(), close()) can yield inside UdpContext::sendTimeout();
// a packet arriving then re-enters LEAmDNS from lwIP on the same UdpContext and both sides free
// the same tx pbufs (#18760). Received packets are only counted during such a call and drained
// from the main loop afterwards.
class GuardedMDNSResponder : public ::esp8266::MDNSImplementation::MDNSResponder {
 public:
  void update_guarded() {
    this->run_guarded_([this]() { this->update(); });
  }
  void close_guarded() {
    this->run_guarded_([this]() { this->close(); });
  }

 private:
  template<typename F> void run_guarded_(F &&fn) {
    UdpContext *ctx = this->m_pUDPContext;
    if (ctx == nullptr) {
      fn();
      return;
    }
    // Set every time: a restart replaces the context together with its stock handler
    ctx->onRx([this]() { this->on_rx_(); });
    this->pending_rx_ = 0;
    this->in_loop_call_ = true;
    fn();
    // Still counting here, so packets arriving during a yield in the drain queue behind it
    while (this->pending_rx_ > 0) {
      this->pending_rx_--;
      this->_process(false);
    }
    this->in_loop_call_ = false;
  }

  // lwIP receive callback (SYS context)
  void on_rx_() {
    if (this->in_loop_call_) {
      this->pending_rx_++;
    } else {
      this->_callProcess();
    }
  }

  volatile bool in_loop_call_{false};
  volatile uint8_t pending_rx_{0};
};

static GuardedMDNSResponder mdns_responder;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void register_esp8266(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
  mdns_responder.begin(App.get_name().c_str());

  for (const auto &service : services) {
    // Strip the leading underscore from the proto and service_type. While it is
    // part of the wire protocol to have an underscore, and for example ESP-IDF
    // expects the underscore to be there, the ESP8266 implementation always adds
    // the underscore itself.
    auto *proto = MDNS_STR_ARG(service.proto);
    while (progmem_read_byte((const uint8_t *) proto) == '_') {
      proto++;
    }
    auto *service_type = MDNS_STR_ARG(service.service_type);
    while (progmem_read_byte((const uint8_t *) service_type) == '_') {
      service_type++;
    }
    uint16_t port = service.port.value();
    mdns_responder.addService(FPSTR(service_type), FPSTR(proto), port);
    for (const auto &record : service.txt_records) {
      mdns_responder.addServiceTxt(FPSTR(service_type), FPSTR(proto), FPSTR(MDNS_STR_ARG(record.key)),
                                   FPSTR(MDNS_STR_ARG(record.value)));
    }
  }
}

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
void MDNSComponent::start_polling_window_() {
  // uint32_t-ID set_interval/set_timeout already does atomic cancel-and-add.
  this->set_interval(MDNS_POLL_ID, MDNS_UPDATE_INTERVAL_MS, []() {
#ifdef USE_MDNS_WIFI_LISTENER
    // Every send fails while the radio is off channel or no interface is up, and each one
    // spins sendTimeout() for 50 ms; skip the tick rather than stall the loop for nothing.
    auto *wifi = wifi::global_wifi_component;
    if (wifi->is_roaming_scan_active() || (!wifi->is_connected() && !wifi->is_ap_active()))
      return;
#endif
    mdns_responder.update_guarded();
  });
  this->set_timeout(MDNS_POLL_STOP_ID, MDNS_POLL_WINDOW_MS, [this]() { this->cancel_interval(MDNS_POLL_ID); });
}
#endif

void MDNSComponent::setup() {
  this->setup_buffers_and_register_(register_esp8266);
#ifdef USE_MDNS_WIFI_LISTENER
  // LEAmDNS's own LwipIntf::statusChangeCB drives _restart() on netif changes; we just
  // arm the window around the initial probe/announce and each reconnect. Unconditional
  // here is safe: setup_priority::AFTER_CONNECTION guarantees the network is up.
  wifi::global_wifi_component->add_ip_state_listener(this);
  this->start_polling_window_();
#endif
}

#ifdef USE_MDNS_WIFI_LISTENER
void MDNSComponent::on_ip_state(const network::IPAddresses &ips, const network::IPAddress &,
                                const network::IPAddress &) {
  // IP listener only fires on acquisition (not loss), so any notification is a fresh
  // IP worth re-arming for. start_polling_window_() is idempotent.
  if (ips[0].is_set()) {
    this->start_polling_window_();
  }
}
#endif

void MDNSComponent::on_shutdown() {
  mdns_responder.close_guarded();
  delay(10);
}

}  // namespace esphome::mdns

#endif

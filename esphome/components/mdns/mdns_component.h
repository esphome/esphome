#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
// Event-driven polling is used whenever the scheduler-backed MDNS.update() interval
// needs to be gated on network state (ESP8266, or RP2040 with WiFi). ESP8266 mDNS
// always runs over WiFi — there is no ethernet driver for ESP8266 in the Arduino
// build — so this path is unconditional on ESP8266. RP2040 can run mDNS over the
// W5500 ethernet shield without WiFi, so it falls back to the legacy polling loop
// when WiFi is absent.
#if defined(USE_ESP8266) || (defined(USE_RP2040) && defined(USE_WIFI) && defined(USE_WIFI_IP_STATE_LISTENERS))
#include "esphome/components/network/ip_address.h"
#include "esphome/components/wifi/wifi_component.h"
#define USE_MDNS_EVENT_DRIVEN_POLLING
#endif

namespace esphome::mdns {

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
/// Call MDNS.update() on the target platform. Defined in the per-platform cpp file so
/// the shared component code can drive the polling window without pulling in
/// platform-specific mDNS headers.
void mdns_pump_update();
#endif

// Helper struct that identifies strings that may be stored in flash storage (similar to LogString)
struct MDNSString;

// Macro to cast string literals to MDNSString* (works on all platforms)
#define MDNS_STR(name) (reinterpret_cast<const esphome::mdns::MDNSString *>(name))

#ifdef USE_ESP8266
#include <pgmspace.h>
#define MDNS_STR_ARG(s) ((PGM_P) (s))
#else
#define MDNS_STR_ARG(s) (reinterpret_cast<const char *>(s))
#endif

// Service count is calculated at compile time by Python codegen
// MDNS_SERVICE_COUNT will always be defined

struct MDNSTXTRecord {
  const MDNSString *key;
  const MDNSString *value;
};

struct MDNSService {
  // service name _including_ underscore character prefix
  // as defined in RFC6763 Section 7
  const MDNSString *service_type;
  // second label indicating protocol _including_ underscore character prefix
  // as defined in RFC6763 Section 7, like "_tcp" or "_udp"
  const MDNSString *proto;
  TemplatableFn<uint16_t> port;
  FixedVector<MDNSTXTRecord> txt_records;
};

class MDNSComponent final : public Component
#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
    ,
                            public wifi::WiFiIPStateListener
#endif
{
 public:
  void setup() override;
  void dump_config() override;

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
  // On ESP8266 and RP2040, MDNS.update() calls _process(true) which only manages
  // timer-driven state machines (probe/announce timeouts and service query cache TTLs).
  // Incoming mDNS packets are handled independently via the lwIP onRx UDP callback and
  // are NOT affected by how often update() is called.
  //
  // The work has a bounded lifetime: after MDNS.begin() (or _restart() triggered by a
  // network interface change) the library sends 3 probes 250ms apart followed by 8
  // announcements 1000ms apart, after which all internal timeouts are set to
  // resetToNeverExpires(). ESPHome does not issue mDNS service queries, so the service
  // query cache is always empty. Every subsequent update() call is pure overhead.
  //
  // Instead of polling forever, we arm a bounded polling window driven by
  // WiFiIPStateListener events. A fresh window covers each probe/announce cycle that
  // follows initial connect or reconnect; outside the window no update() calls occur.
  static constexpr uint32_t MDNS_UPDATE_INTERVAL_MS = 50;
  // Boot probe+announce phase is ~9.0s (3*250ms probes + 8*1000ms announces). Window
  // includes margin for the initial `rand() % MDNS_PROBE_DELAY` jitter and for the
  // debounced internal restart triggered by netif status changes on ESP8266.
  static constexpr uint32_t MDNS_POLL_WINDOW_MS = 12000;
  // Scheduler IDs (uint32_t variants avoid name hashing/strcmp on cancel paths)
  static constexpr uint32_t MDNS_POLL_ID = 0;
  static constexpr uint32_t MDNS_POLL_STOP_ID = 1;
#endif
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

#ifdef USE_MDNS_EXTRA_SERVICES
  void add_extra_service(MDNSService service) { this->services_.emplace_next() = std::move(service); }
#endif

#ifdef USE_MDNS_STORE_SERVICES
  const StaticVector<MDNSService, MDNS_SERVICE_COUNT> &get_services() const { return this->services_; }
#endif

  void on_shutdown() override;

#ifdef USE_MDNS_DYNAMIC_TXT
  /// Add a dynamic TXT value and return pointer to it for use in MDNSTXTRecord
  const char *add_dynamic_txt_value(const std::string &value) {
    this->dynamic_txt_values_.push_back(value);
    return this->dynamic_txt_values_[this->dynamic_txt_values_.size() - 1].c_str();
  }
#endif

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
  void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                   const network::IPAddress &dns2) override;
#endif

 protected:
#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
  /// Arm a bounded polling window so MDNS.update() runs at MDNS_UPDATE_INTERVAL_MS
  /// for MDNS_POLL_WINDOW_MS. A subsequent call replaces the previous window.
  void start_polling_window_();
  /// Cancel any active polling window.
  void cancel_polling_window_();
  bool ip_was_up_{false};
#endif
  /// Helper to set up services and MAC buffers, then call platform-specific registration
  using PlatformRegisterFn = void (*)(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &);

  void setup_buffers_and_register_(PlatformRegisterFn platform_register) {
#ifdef USE_MDNS_STORE_SERVICES
    auto &services = this->services_;
#else
    StaticVector<MDNSService, MDNS_SERVICE_COUNT> services_storage;
    auto &services = services_storage;
#endif

#ifdef USE_API
#ifdef USE_MDNS_STORE_SERVICES
    get_mac_address_into_buffer(this->mac_address_);
    char *mac_ptr = this->mac_address_;
#else
    char mac_address[MAC_ADDRESS_BUFFER_SIZE];
    get_mac_address_into_buffer(mac_address);
    char *mac_ptr = mac_address;
#endif
#else
    char *mac_ptr = nullptr;
#endif

    this->compile_records_(services, mac_ptr);
    platform_register(this, services);
  }

#ifdef USE_MDNS_DYNAMIC_TXT
  /// Storage for runtime-generated TXT values from user lambdas
  /// Pre-sized at compile time via MDNS_DYNAMIC_TXT_COUNT to avoid heap allocations.
  /// Static/compile-time values (version, board, etc.) are stored directly in flash and don't use this.
  StaticVector<std::string, MDNS_DYNAMIC_TXT_COUNT> dynamic_txt_values_;
#endif

#if defined(USE_API) && defined(USE_MDNS_STORE_SERVICES)
  /// Fixed buffer for MAC address (only needed when services are stored)
  char mac_address_[MAC_ADDRESS_BUFFER_SIZE];
#endif
#ifdef USE_MDNS_STORE_SERVICES
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services_{};
#endif
#ifdef USE_RP2040
  bool initialized_{false};
#if !defined(USE_MDNS_EVENT_DRIVEN_POLLING)
  bool was_connected_{false};
#endif
#endif
  void compile_records_(StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services, char *mac_address_buf);
};

}  // namespace esphome::mdns
#endif

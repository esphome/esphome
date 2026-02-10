#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::mdns {

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
  TemplatableValue<uint16_t> port;
  FixedVector<MDNSTXTRecord> txt_records;
};

class MDNSComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;

  // Polling interval for MDNS.update() on platforms that require it (ESP8266, RP2040).
  //
  // On these platforms, MDNS.update() calls _process(true) which only manages timer-driven
  // state machines (probe/announce timeouts and service query cache TTLs). Incoming mDNS
  // packets are handled independently via the lwIP onRx UDP callback and are NOT affected
  // by how often update() is called.
  //
  // The shortest internal timer is the 250ms probe interval (RFC 6762 Section 8.1).
  // Announcement intervals are 1000ms and cache TTL checks are on the order of seconds
  // to minutes. A 50ms polling interval provides sufficient resolution for all timers
  // while completely removing mDNS from the per-iteration loop list.
  //
  // In steady state (after the ~8 second boot probe/announce phase completes), update()
  // checks timers that are set to never expire, making every call pure overhead.
  //
  // Tasmota uses a 50ms main loop cycle with mDNS working correctly, confirming this
  // interval is safe in production.
  //
  // By using set_interval() instead of overriding loop(), the component is excluded from
  // the main loop list via has_overridden_loop(), eliminating all per-iteration overhead
  // including virtual dispatch.
  static constexpr uint32_t MDNS_UPDATE_INTERVAL_MS = 50;
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

 protected:
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
  void compile_records_(StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services, char *mac_address_buf);
};

}  // namespace esphome::mdns
#endif

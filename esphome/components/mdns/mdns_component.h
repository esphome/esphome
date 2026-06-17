#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
// On ESP8266 and RP2040 the scheduler-backed MDNS.update() polling window is armed by
// IP state listener events on whichever network interface is configured.
#if (defined(USE_ESP8266) || defined(USE_RP2040)) && \
    ((defined(USE_WIFI) && defined(USE_WIFI_IP_STATE_LISTENERS)) || \
     (defined(USE_ETHERNET) && defined(USE_ETHERNET_IP_STATE_LISTENERS)))
#include "esphome/components/network/ip_address.h"
#define USE_MDNS_EVENT_DRIVEN_POLLING
#if defined(USE_WIFI) && defined(USE_WIFI_IP_STATE_LISTENERS)
#include "esphome/components/wifi/wifi_component.h"
#define USE_MDNS_WIFI_LISTENER
#endif
#if defined(USE_ETHERNET) && defined(USE_ETHERNET_IP_STATE_LISTENERS)
#include "esphome/components/ethernet/ethernet_component.h"
#define USE_MDNS_ETHERNET_LISTENER
#endif
#endif

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
  TemplatableFn<uint16_t> port;
  FixedVector<MDNSTXTRecord> txt_records;
};

class MDNSComponent final : public Component
#ifdef USE_MDNS_WIFI_LISTENER
    ,
                            public wifi::WiFiIPStateListener
#endif
#ifdef USE_MDNS_ETHERNET_LISTENER
    ,
                            public ethernet::EthernetIPStateListener
#endif
{
 public:
  void setup() override;
  void dump_config() override;

  /// Size of buffer required for config hash hex string (8 hex chars + null terminator)
  static constexpr size_t CONFIG_HASH_STR_SIZE = format_hex_size(sizeof(uint32_t));

#ifdef USE_MDNS_EVENT_DRIVEN_POLLING
  // LEAmDNS has meaningful work only during the probe+announce phase (3×250ms probes +
  // 8×1000ms announces, ~9s). Afterwards every internal timer is resetToNeverExpires()
  // and update() becomes pure overhead. We arm a bounded polling window from IP state
  // listener events so update() runs only during that phase.
  static constexpr uint32_t MDNS_UPDATE_INTERVAL_MS = 50;
  // Must exceed LEAmDNS's longest restart-to-announce-complete path:
  //   MDNS_PROBE_DELAY (250ms) × MDNS_PROBE_COUNT (3) = 750ms probing
  // + MDNS_ANNOUNCE_DELAY (1000ms) × MDNS_ANNOUNCE_COUNT (8) = 8000ms announcing
  // + rand() % MDNS_PROBE_DELAY jitter on first probe (0–250ms)
  // + debounced schedule_function() hop when statusChangeCB fires on ESP8266
  // ≈ 9s nominal. 15s gives ~6s margin to absorb main-loop blocking (long
  // component setup, WiFi scan, flash writes) that could stretch the deadlines
  // between our polls. If LEAmDNS ever extends its phase (upstream library
  // update) this constant needs to grow. Constants defined in LEAmDNS_Priv.h
  // (ESP8266 core 3.1.2 / arduino-pico 5.5.1).
  static constexpr uint32_t MDNS_POLL_WINDOW_MS = 15000;
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
  /// Arm a fresh MDNS_POLL_WINDOW_MS polling window. Idempotent — re-arming replaces
  /// the previous window via the scheduler's atomic cancel-and-add on matching IDs.
  void start_polling_window_();
#endif
  /// Helper to set up services and MAC buffers, then call platform-specific registration
  using PlatformRegisterFn = void (*)(MDNSComponent *, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &);

  void setup_buffers_and_register_(PlatformRegisterFn platform_register);

#ifdef USE_MDNS_DYNAMIC_TXT
  /// Storage for runtime-generated TXT values from user lambdas
  /// Pre-sized at compile time via MDNS_DYNAMIC_TXT_COUNT to avoid heap allocations.
  /// Static/compile-time values (version, board, etc.) are stored directly in flash and don't use this.
  StaticVector<std::string, MDNS_DYNAMIC_TXT_COUNT> dynamic_txt_values_;
#endif

#if defined(USE_API) && defined(USE_MDNS_STORE_SERVICES)
  /// Fixed buffer for MAC address (only needed when services are stored)
  char mac_address_[MAC_ADDRESS_BUFFER_SIZE];
  /// Fixed buffer for config hash hex string (only needed when services are stored)
  char config_hash_str_[CONFIG_HASH_STR_SIZE];
#endif
#ifdef USE_MDNS_STORE_SERVICES
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services_{};
#endif
#if defined(USE_RP2040) && defined(USE_MDNS_EVENT_DRIVEN_POLLING)
  // RP2040 defers MDNS.begin() until the first IP-up event; this tracks that.
  bool initialized_{false};
#endif
  void compile_records_(StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services, char *mac_address_buf,
                        char *config_hash_buf);
};

}  // namespace esphome::mdns
#endif

#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mdns {

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

#if (defined(USE_ESP8266) || defined(USE_RP2040)) && defined(USE_ARDUINO)
  void loop() override;
#endif
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

#ifdef USE_MDNS_EXTRA_SERVICES
  void add_extra_service(MDNSService service) { this->services_.emplace_next() = std::move(service); }
#endif

#ifdef USE_MDNS_STORE_SERVICES
  const StaticVector<MDNSService, MDNS_SERVICE_COUNT> &get_services() const { return this->services_; }
#endif

  void on_shutdown() override;

  /// Add a dynamic TXT value and return pointer to it for use in MDNSTXTRecord
  const char *add_dynamic_txt_value(const std::string &value) {
    this->dynamic_txt_values_.push_back(value);
    return this->dynamic_txt_values_[this->dynamic_txt_values_.size() - 1].c_str();
  }

  /// Storage for runtime-generated TXT values (MAC address, user lambdas)
  /// Pre-sized at compile time via MDNS_DYNAMIC_TXT_COUNT to avoid heap allocations.
  /// Static/compile-time values (version, board, etc.) are stored directly in flash and don't use this.
  StaticVector<std::string, MDNS_DYNAMIC_TXT_COUNT> dynamic_txt_values_;

 protected:
#ifdef USE_MDNS_STORE_SERVICES
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services_{};
#endif
  void compile_records_(StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services);
};

}  // namespace mdns
}  // namespace esphome
#endif

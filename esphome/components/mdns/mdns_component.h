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
  TemplatableValue<std::string> value;
};

struct MDNSService {
  // service name _including_ underscore character prefix
  // as defined in RFC6763 Section 7
  const MDNSString *service_type;
  // second label indicating protocol _including_ underscore character prefix
  // as defined in RFC6763 Section 7, like "_tcp" or "_udp"
  const MDNSString *proto;
  TemplatableValue<uint16_t> port;
  std::vector<MDNSTXTRecord> txt_records;
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

  const StaticVector<MDNSService, MDNS_SERVICE_COUNT> &get_services() const { return this->services_; }

  void on_shutdown() override;

 protected:
  StaticVector<MDNSService, MDNS_SERVICE_COUNT> services_{};
  std::string hostname_;
  void compile_records_();
};

}  // namespace mdns
}  // namespace esphome
#endif

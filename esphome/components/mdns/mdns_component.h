#pragma once
#include "esphome/core/defines.h"
#ifdef USE_MDNS
#include <string>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mdns {

// Service count is calculated at compile time by Python codegen
// MDNS_SERVICE_COUNT will always be defined

struct MDNSTXTRecord {
  std::string key;
  TemplatableValue<std::string> value;
};

struct MDNSService {
  // service name _including_ underscore character prefix
  // as defined in RFC6763 Section 7
  std::string service_type;
  // second label indicating protocol _including_ underscore character prefix
  // as defined in RFC6763 Section 7, like "_tcp" or "_udp"
  std::string proto;
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

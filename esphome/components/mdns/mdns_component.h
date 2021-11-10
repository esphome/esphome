#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"

namespace esphome {
namespace mdns {

struct MDNSTXTRecord {
  std::string key;
  std::string value;
};

struct MDNSService {
  // service name _including_ underscore character prefix
  // as defined in RFC6763 Section 7
  std::string service_type;
  // second label indicating protocol _including_ underscore character prefix
  // as defined in RFC6763 Section 7, like "_tcp" or "_udp"
  std::string proto;
  uint16_t port;
  std::vector<MDNSTXTRecord> txt_records;
};

class MDNSComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;

#if defined(USE_ESP8266) && defined(USE_ARDUINO)
  void loop() override;
#endif
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  std::vector<MDNSService> services_{};
  std::string hostname_;
  void compile_records_();
};

}  // namespace mdns
}  // namespace esphome

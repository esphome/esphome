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
  std::string service_type;
  std::string proto;
  uint16_t port;
  std::vector<MDNSTXTRecord> txt_records;
};

class MDNSComponent : public Component {
 public:
  void setup() override;

#if defined(USE_ESP8266) && defined(USE_ARDUINO)
  void loop() override;
#endif
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  std::vector<MDNSService> compile_services_();
  std::string compile_hostname_();
};

}  // namespace mdns
}  // namespace esphome

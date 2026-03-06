#pragma once
#ifdef USE_ESP_IDF

#include "esphome/components/nmea/nmea.h"

namespace esphome {
namespace nmea {

class ModemNMEAComponent : public NMEAComponent {
 public:
  void set_gnss_command(const std::string &gnss_command) { this->gnss_command_ = gnss_command; }

  void update() override;

 protected:
  std::string gnss_command_;
};

}  // namespace nmea
}  // namespace esphome

#endif  // USE_ESP_IDF

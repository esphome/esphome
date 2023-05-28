#ifndef NTP_SERVER_H
#define NTP_SERVER_H

#include "esphome/core/component.h"

namespace esphome {
namespace ntp_server {

class NtpServer : public Component {
public:
  void setup() override; // called once
  void loop() override;  // called frequently
};

} // namespace ntp_server
} // namespace esphome

#endif

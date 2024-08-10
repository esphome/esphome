#pragma once

#include "muart_packet.h"

namespace esphome {
namespace mitsubishi_itp {

static constexpr char LISTENER_TAG[] = "mitsubishi_itp.listener";

class MITPListener : public PacketProcessor {
 public:
  virtual void publish(bool force = true) = 0;

  // TODO: These trhee are only used by the TemperatureSourceSelect, so might need to be broken out (putting them here
  // now to get things working)
  virtual void setup(bool thermostat_is_present){};  // Called during hub-component setup();
  virtual void temperature_source_change(const std::string temp_source){};
};

}  // namespace mitsubishi_itp
}  // namespace esphome

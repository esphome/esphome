#pragma once

#include "muart_packet.h"

namespace esphome {
namespace mitsubishi_itp {

static constexpr char LISTENER_TAG[] = "mitsubishi_itp.listener";

class MITPListener : public PacketProcessor {
 public:
  virtual void publish(bool force = true) = 0;
};

}  // namespace mitsubishi_itp
}  // namespace esphome

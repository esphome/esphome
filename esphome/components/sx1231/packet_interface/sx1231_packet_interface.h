#pragma once

#include "esphome/components/packet_interface/radio_link.h"
#include "esphome/components/sx1231/sx1231.h"

namespace esphome::sx1231 {

using SX1231PacketInterface = packet_interface::RadioLink<SX1231, SX1231Listener, SX1231Error, SX1231_MAX_PACKET_SIZE>;

}  // namespace esphome::sx1231

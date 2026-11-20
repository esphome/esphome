#pragma once

#include "esphome/components/packet_interface/radio_link.h"
#include "esphome/components/sx126x/sx126x.h"

namespace esphome::sx126x {

using SX126xPacketInterface = packet_interface::RadioLink<SX126x, SX126xListener, SX126xError, SX126X_MAX_PACKET_SIZE>;

}  // namespace esphome::sx126x

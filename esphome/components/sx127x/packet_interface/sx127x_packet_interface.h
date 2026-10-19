#pragma once

#include "esphome/components/packet_interface/radio_link.h"
#include "esphome/components/sx127x/sx127x.h"

namespace esphome::sx127x {

using SX127xPacketInterface = packet_interface::RadioLink<SX127x, SX127xListener, SX127xError, SX127X_MAX_PACKET_SIZE>;

}  // namespace esphome::sx127x

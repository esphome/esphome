#pragma once
#include "mbus-frame.h"

namespace esphome {
namespace mbus {

class MBusFrameFactory {
 public:
  static MBusFrame CreateNKEFrame(uint8_t primary_address);
};

}  // namespace mbus
}  // namespace esphome

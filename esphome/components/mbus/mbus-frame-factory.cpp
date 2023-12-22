#include "mbus-frame-factory.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mbus {

MBusFrame MBusFrameFactory::CreateNKEFrame(uint8_t primary_address) {
  MBusFrame frame(MBUS_FRAME_TYPE_SHORT);

  frame.control = MBusControlCodes::SND_NKE;
  frame.address = primary_address;

  return frame;
}

}  // namespace mbus
}  // namespace esphome

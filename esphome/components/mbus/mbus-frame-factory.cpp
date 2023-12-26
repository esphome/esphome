#include "mbus-frame-factory.h"
#include "mbus-frame-meta.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mbus {

std::unique_ptr<MBusFrame> MBusFrameFactory::CreateNKEFrame(uint8_t primary_address) {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_SHORT);

  frame->control = MBusControlCodes::SND_NKE;
  frame->address = primary_address;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::CreateSlaveSelect(std::vector<uint8_t> mask) {
  assert(mask.size() == 8);

  auto frame = new MBusFrame(MBUS_FRAME_TYPE_LONG);

  frame->control = MBusControlCodes::SND_UD_MASTER;
  frame->address = MBusAddresses::NETWORK_LAYER;
  frame->control_information = MBusControlInformationCodes::SELECTION_OF_SLAVES;
  frame->data = mask;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

}  // namespace mbus
}  // namespace esphome

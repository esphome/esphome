#include "mbus-frame-factory.h"
#include "mbus-frame-meta.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mbus {

std::unique_ptr<MBusFrame> MBusFrameFactory::create_empty_frame() {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_EMPTY);

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

// request frames
std::unique_ptr<MBusFrame> MBusFrameFactory::create_nke_frame(uint8_t primary_address) {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_SHORT);

  frame->control = MBusControlCodes::SND_NKE;
  frame->address = primary_address;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_req_ud2_frame() {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_SHORT);

  frame->control = MBusControlCodes::REQ_UD2;
  frame->address = MBusAddresses::NETWORK_LAYER;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_slave_select(std::vector<uint8_t> &mask) {
  assert(mask.size() == 8);

  auto frame = new MBusFrame(MBUS_FRAME_TYPE_LONG);

  frame->control = MBusControlCodes::SND_UD_MASTER;
  frame->address = MBusAddresses::NETWORK_LAYER;
  frame->control_information = MBusControlInformationCodes::SELECTION_OF_SLAVES_MODE1;
  frame->data = mask;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

// response frames
std::unique_ptr<MBusFrame> MBusFrameFactory::create_ack_frame() {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_ACK);

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}
std::unique_ptr<MBusFrame> MBusFrameFactory::create_short_frame(uint8_t control, uint8_t address, uint8_t checksum) {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_SHORT);
  frame->control = control;
  frame->address = address;
  frame->checksum = checksum;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_control_frame(uint8_t control, uint8_t address,
                                                                  uint8_t control_information, uint8_t checksum) {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_CONTROL);
  frame->control = control;
  frame->address = address;
  frame->control_information = control_information;
  frame->checksum = checksum;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_long_frame(uint8_t control, uint8_t address,
                                                               uint8_t control_information, std::vector<uint8_t> &data,
                                                               uint8_t checksum) {
  auto frame = new MBusFrame(MBUS_FRAME_TYPE_LONG);
  frame->control = control;
  frame->address = address;
  frame->control_information = control_information;
  frame->data = data;
  frame->checksum = checksum;

  std::unique_ptr<MBusFrame> frame_ptr(frame);
  return frame_ptr;
}

}  // namespace mbus
}  // namespace esphome

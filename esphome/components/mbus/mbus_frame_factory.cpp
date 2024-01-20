
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "mbus_decoder.h"
#include "mbus_frame_factory.h"
#include "mbus_frame_meta.h"

namespace esphome {
namespace mbus {

std::unique_ptr<MBusFrame> MBusFrameFactory::create_empty_frame() {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_EMPTY);

  return frame;
}

// request frames
std::unique_ptr<MBusFrame> MBusFrameFactory::create_nke_frame(uint8_t primary_address) {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_SHORT);

  frame->control = MBusControlCodes::SND_NKE;
  frame->address = primary_address;

  return frame;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_req_ud2_frame() {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_SHORT);

  frame->control = MBusControlCodes::REQ_UD2;
  frame->address = MBusAddresses::NETWORK_LAYER;

  return frame;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_slave_select(uint64_t secondary_address) {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_LONG);

  frame->control = MBusControlCodes::SND_UD_MASTER;
  frame->address = MBusAddresses::NETWORK_LAYER;
  frame->control_information = MBusControlInformationCodes::SELECTION_OF_SLAVES_MODE1;

  MBusDecoder::encode_secondary_address(secondary_address, &(frame->data));

  return frame;
}

// response frames
std::unique_ptr<MBusFrame> MBusFrameFactory::create_ack_frame() {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_ACK);

  return frame;
}
std::unique_ptr<MBusFrame> MBusFrameFactory::create_short_frame(uint8_t control, uint8_t address, uint8_t checksum) {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_SHORT);
  frame->control = control;
  frame->address = address;
  frame->checksum = checksum;

  return frame;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_control_frame(uint8_t control, uint8_t address,
                                                                  uint8_t control_information, uint8_t checksum) {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_CONTROL);
  frame->control = control;
  frame->address = address;
  frame->control_information = control_information;
  frame->checksum = checksum;

  return frame;
}

std::unique_ptr<MBusFrame> MBusFrameFactory::create_long_frame(uint8_t control, uint8_t address,
                                                               uint8_t control_information, std::vector<uint8_t> &data,
                                                               uint8_t checksum) {
  auto frame = make_unique<MBusFrame>(MBUS_FRAME_TYPE_LONG);
  frame->control = control;
  frame->address = address;
  frame->control_information = control_information;
  frame->data = data;
  frame->checksum = checksum;

  return frame;
}

}  // namespace mbus
}  // namespace esphome

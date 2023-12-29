#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace mbus {

#define MBUS_FRAME_DATA_LENGTH 252

enum MBusFrameType {
  MBUS_FRAME_TYPE_EMPTY = 0x00,
  MBUS_FRAME_TYPE_ACK = 0x01,
  MBUS_FRAME_TYPE_SHORT = 0x02,
  MBUS_FRAME_TYPE_CONTROL = 0x03,
  MBUS_FRAME_TYPE_LONG = 0x04,
};

class MBusFrame {
 public:
  uint8_t start;
  uint8_t length;
  uint8_t control;
  uint8_t control_information;
  uint8_t address;
  uint8_t checksum;
  uint8_t stop;
  std::vector<uint8_t> data;

  MBusFrameType frame_type;

  // void *next;  // pointer to next mbus_frame for multi-telegram replies

  MBusFrame(MBusFrameType frame_type);
  MBusFrame(MBusFrame &frame);

  static uint8_t serialize(MBusFrame &frame, std::vector<uint8_t> &buffer);
  static uint8_t calc_length(MBusFrame &frame);
  static uint8_t calc_checksum(MBusFrame &frame);

  void dump();
};

}  // namespace mbus
}  // namespace esphome

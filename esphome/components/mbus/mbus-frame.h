#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace mbus {

#define MBUS_FRAME_DATA_LENGTH 252

//
// Frame start/stop bits
//
#define MBUS_FRAME_ACK_START 0xE5
#define MBUS_FRAME_SHORT_START 0x10
#define MBUS_FRAME_CONTROL_START 0x68
#define MBUS_FRAME_LONG_START 0x68
#define MBUS_FRAME_STOP 0x16

//
// Frame base sizes
//

class MBusFrameBaseSize {
 public:
  static const uint8_t ACK_SIZE = 1;
  static const uint8_t SHORT_SIZE = 5;
  static const uint8_t CONTROL_SIZE = 9;
  static const uint8_t LONG_SIZE = 9;
};

enum MBusFrameType {
  MBUS_FRAME_TYPE_ACK = 0x01,
  MBUS_FRAME_TYPE_SHORT = 0x02,
  MBUS_FRAME_TYPE_CONTROL = 0x03,
  MBUS_FRAME_TYPE_LONG = 0x04,
};

class MBusControlCodes {
 public:
  // MBUS_CONTROL_MASK_DIR_S2M = 0x00,
  // MBUS_CONTROL_FIELD_F0 = 0x01,
  // MBUS_CONTROL_FIELD_F1 = 0x02,
  // MBUS_CONTROL_FIELD_F2 = 0x03,
  // MBUS_CONTROL_FIELD_F3 = 0x04,
  // MBUS_CONTROL_FIELD_DFC = 0x05,
  // MBUS_CONTROL_FIELD_FCV = 0x05,
  // MBUS_CONTROL_FIELD_ACD = 0x06,
  // MBUS_CONTROL_FIELD_FCB = 0x06,
  // MBUS_CONTROL_FIELD_DIRECTION = 0x07,
  // MBUS_CONTROL_MASK_DFC = 0x10,
  // MBUS_CONTROL_MASK_FCV = 0x10,
  // MBUS_CONTROL_MASK_ACD = 0x20,
  // MBUS_CONTROL_MASK_FCB = 0x20,
  static const uint8_t SND_NKE = 0x40;
  static const uint8_t SND_UD = 0x53;
  static const uint8_t REQ_UD1 = 0x5A;
  static const uint8_t REQ_UD2 = 0x5B;
  static const uint8_t RSP_UD = 0x08;
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

  // unsigned char data[MBUS_FRAME_DATA_LENGTH];
  // size_t data_size;

  MBusFrameType frame_type;
  // time_t timestamp;

  // mbus_frame_data frame_data;

  void *next;  // pointer to next mbus_frame for multi-telegram replies

  MBusFrame(MBusFrameType frame_type);
  static uint8_t serialize(MBusFrame &frame, std::vector<uint8_t> &buffer);
  static uint8_t calc_length(MBusFrame &frame);
  static uint8_t calc_checksum(MBusFrame &frame);
};

}  // namespace mbus
}  // namespace esphome

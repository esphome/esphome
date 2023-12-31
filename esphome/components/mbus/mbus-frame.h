#pragma once

#include <memory>
#include <vector>

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

class MBusDataVariable;
class MBusDataVariableHeader;

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
  std::unique_ptr<MBusDataVariable> variable_data;

  MBusFrameType frame_type;

  // void *next;  // pointer to next mbus_frame for multi-telegram replies

  MBusFrame(MBusFrameType frame_type);
  MBusFrame(MBusFrame &frame);

  static uint8_t serialize(MBusFrame &frame, std::vector<uint8_t> &buffer);
  static uint8_t calc_length(MBusFrame &frame);
  static uint8_t calc_checksum(MBusFrame &frame);

  void dump();
};

// Ident.Nr.   Manufr. Version Medium Access No. Status  Signature
// 4 Byte BCD  2 Byte  1 Byte  1 Byte   1 Byte   1 Byte  2 Byte
class MBusDataVariableHeader {
 public:
  uint64_t id;
  std::string manufacturer;
  uint8_t version;
  uint8_t medium;
  uint8_t access_no;
  uint8_t status;
  uint8_t signature[2];
};

class MBusDataVariable {
 public:
  MBusDataVariableHeader header;

  // mbus_data_record *record;
  // size_t nrecords;

  std::vector<uint8_t> data;
  // size_t data_len;

  uint8_t more_records_follow;

  // are these needed/used?
  // unsigned char mdh;
  // unsigned char *mfg_data;
  // size_t mfg_data_len;

  void dump();
};

}  // namespace mbus
}  // namespace esphome

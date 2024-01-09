#pragma once

#include <memory>
#include <vector>
#include <string>

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

// MBus Data
class MBusDataDifBit {
 public:
  static const uint8_t DIF_EXTENSION_BIT = 0x80;
  static const uint8_t VIF_EXTENSION_BIT = 0x80;
  static const uint8_t DIF_IDLE_FILLER = 0x2F;
  static const uint8_t DIF_MANUFACTURER_SPECIFIC = 0x0F;
  static const uint8_t DIF_MORE_RECORDS_FOLLOW = 0x1F;
};

// DIF (Data Information Field)
// |   Bit 7   |    6    |    5       4    |  3     2     1     0 |
// | Extension | LSB of  |  Function Field |  Data Field:         |
// | Bit       | storage |                 |  Length and coding   |
// |           | number  |                 |  of data             |
//
// DIFE (Data Information Field Extension)
// |   Bit 7   |    6    |    5       4    |  3     2     1     0 |
// | Extension | Device  |      Tariff     |    Storage Number    |
// | Bit       |  Unit   |                 |                      |
class MBusDataInformationBlock {
 public:
  uint8_t dif;
  std::vector<uint8_t> dife;
};

// VIF (VAlue Information Field)
// |   Bit 7   | 6      5       4       3       2       1      0  |
// | Extension |                Unit and Multiplier               |
// | Bit       |                                                  |
class MBusValueInformationBlock {
 public:
  uint8_t vif;
  std::vector<uint8_t> vife;
};

class MBusDataRecordHeader {
 public:
  MBusDataInformationBlock dib;
  MBusValueInformationBlock vib;
};

class MBusDataRecord {
 public:
  MBusDataRecordHeader drh;
  std::vector<uint8_t> data;

  // time_t timestamp;

  // void *next;
};

// Ident.Nr.   Manufr. Version Medium Access No. Status  Signature
// 4 Byte BCD  2 Byte  1 Byte  1 Byte   1 Byte   1 Byte  2 Byte
class MBusDataVariableHeader {
 public:
  uint8_t id[4];
  uint8_t manufacturer[2];
  uint8_t version;
  uint8_t medium;
  uint8_t access_no;
  uint8_t status;
  uint8_t signature[2];

  std::string get_secondary_address();
};

class MBusDataVariable {
 public:
  MBusDataVariableHeader header;
  std::vector<MBusDataRecord> records;

  // uint8_t more_records_follow;

  // are these needed/used?
  // uint8_t mdh;
  // std::vector<uint8_t> mfg_data;

  void dump();
};

}  // namespace mbus
}  // namespace esphome

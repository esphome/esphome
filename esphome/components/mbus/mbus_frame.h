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

enum MBusDataType {
  NO_DATA = 0x00,
  INT8 = 0x01,
  INT16 = 0x02,
  INT24 = 0x03,
  INT32 = 0x04,
  FLOAT = 0x05,
  INT48 = 0x06,
  INT64 = 0x07,
  BCD_8 = 0x08,
  BCD_16 = 0x09,
  BCD_24 = 0x10,
  BCD_32 = 0x11,
  BCD_48 = 0x12,
  SPECIAL = 0x13,
  VARIABLE = 0x14,
  DATE_16 = 0x15,
  DATE_TIME_32 = 0x16,
  DATE_TIME_48 = 0x17,
};

class MBusDataVariable;
class MBusDataVariableHeader;
class MBusValue;

class MBusFrame {
 public:
  uint8_t start{0};
  uint8_t length{0};
  uint8_t control{0};
  uint8_t control_information{0};
  uint8_t address{0};
  uint8_t checksum{0};
  uint8_t stop{0};
  std::vector<uint8_t> data;
  std::unique_ptr<MBusDataVariable> variable_data{nullptr};

  MBusFrameType frame_type{MBusFrameType::MBUS_FRAME_TYPE_EMPTY};

  // void *next;  // pointer to next mbus_frame for multi-telegram replies

  MBusFrame(MBusFrameType frame_type);
  MBusFrame(MBusFrame &frame);

  static uint8_t serialize(MBusFrame &frame, std::vector<uint8_t> &buffer);
  static uint8_t calc_length(MBusFrame &frame);
  static uint8_t calc_checksum(MBusFrame &frame);

  void dump() const;
  void dump_frame() const;
  void dump_frame_type() const;
};

// MBus Data
class MBusDataDifMask {
 public:
  static const uint8_t TARIFF = 0x30;
  static const uint8_t EXTENSION_BIT = 0x80;
  static const uint8_t IDLE_FILLER = 0x2F;
  static const uint8_t MANUFACTURER_SPECIFIC = 0x0F;
  static const uint8_t FUNCTION = 0x30;
  static const uint8_t MORE_RECORDS_FOLLOW = 0x1F;
  static const uint8_t DATA_CODING = 0x0F;
};

class MBusDataVifMask {
 public:
  static const uint8_t EXTENSION_BIT = 0x80;
  static const uint8_t UNIT_AND_MULTIPLIER = 0x7F;
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
  uint8_t dif{0};
  std::vector<uint8_t> dife;
};

// VIF (VAlue Information Field)
// |   Bit 7   | 6      5       4       3       2       1      0  |
// | Extension |                Unit and Multiplier               |
// | Bit       |                                                  |
class MBusValueInformationBlock {
 public:
  uint8_t vif{0};
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

  std::unique_ptr<MBusValue> parse(const uint8_t id);
  // void *next;

 protected:
  uint32_t parse_tariff(const MBusDataRecord *record);
  std::string parse_function(const MBusDataRecord *record);
  std::string parse_unit(const MBusDataRecord *record);
  std::string parse_date_time_unit(const uint8_t exponent);
  MBusDataType parse_data_type(const MBusDataRecord *record);
  float parse_value(const MBusDataRecord *record, const MBusDataType &data_type);
};

// Ident.Nr.   Manufr. Version Medium Access No. Status  Signature
// 4 Byte BCD  2 Byte  1 Byte  1 Byte   1 Byte   1 Byte  2 Byte
class MBusDataVariableHeader {
 public:
  uint8_t id[4]{0, 0, 0, 0};
  uint8_t manufacturer[2]{0, 0};
  uint8_t version{0};
  uint8_t medium{0};
  uint8_t access_no{0};
  uint8_t status{0};
  uint8_t signature[2]{0, 0};
};

class MBusDataVariable {
 public:
  MBusDataVariableHeader header;
  std::vector<MBusDataRecord> records;

  // bool more_records_follow;

  // uint8_t mdh;
  // std::vector<uint8_t> mfg_data;

  void dump() const;
};

class MBusValue {
 public:
  uint8_t id{0};
  std::string function{""};
  std::string unit{""};
  float value{0.0};
  uint8_t tariff{0};
  MBusDataType data_type{MBusDataType::NO_DATA};

  std::string get_data_type_str() const {
    switch (this->data_type) {
      case 0x00:
        return "NO_DATA";
      case 0x01:
        return "INT8";
      case 0x02:
        return "INT16";
      case 0x03:
        return "INT24";
      case 0x04:
        return "INT32";
      case 0x05:
        return "FLOAT";
      case 0x06:
        return "INT48";
      case 0x07:
        return "INT64";
      case 0x08:
        return "BCD_8";
      case 0x09:
        return "BCD_16";
      case 0x10:
        return "BCD_24";
      case 0x11:
        return "BCD_32";
      case 0x12:
        return "BCD_48";
      case 0x13:
        return "SPECIAL";
      case 0x14:
        return "VARIABLE";
      case 0x15:
        return "DATE_16";
      case 0x16:
        return "DATE_TIME_32";
      case 0x17:
        return "DATE_TIME_48";
    }

    return "unknown data type";
  }
};

}  // namespace mbus
}  // namespace esphome

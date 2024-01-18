#pragma once
#include "stdint.h"

namespace esphome {
namespace mbus {

class MBusAddresses {
 public:
  static const uint8_t NETWORK_LAYER = 0xFD;
  static const uint8_t TEST_ADDRESS = 0xFE;
  static const uint8_t BROADCAST = 0xFF;
};

class MBusControlCodes {
 public:
  static const uint8_t SND_NKE = 0x40;
  static const uint8_t SND_UD_SLAVE = 0x53;
  static const uint8_t SND_UD_MASTER = 0x73;
  static const uint8_t REQ_UD1 = 0x5A;
  static const uint8_t REQ_UD2 = 0x5B;
  static const uint8_t RSP_UD = 0x08;
};

class MBusControlInformationCodes {
 public:
  static const uint8_t SELECTION_OF_SLAVES_MODE1 = 0x52;
  static const uint8_t VARIABLE_DATA_RESPONSE_MODE1 = 0x72;
};
class MBusFrameMeta {
 public:
  const uint8_t start_bit;
  const uint8_t stop_bit;
  const uint8_t lenght;
  const uint8_t base_frame_size;

  MBusFrameMeta(uint8_t _start_bit, uint8_t _stop_bit, uint8_t _length, uint8_t _base_frame_size)
      : start_bit(_start_bit), stop_bit(_stop_bit), lenght(_length), base_frame_size(_base_frame_size) {}
};

class MBusFrameDefinition {
 public:
  static const uint8_t MAX_DATA_LENGTH{252};
  static const MBusFrameMeta ACK_FRAME;
  static const MBusFrameMeta SHORT_FRAME;
  static const MBusFrameMeta CONTROL_FRAME;
  static const MBusFrameMeta LONG_FRAME;
};

}  // namespace mbus
}  // namespace esphome

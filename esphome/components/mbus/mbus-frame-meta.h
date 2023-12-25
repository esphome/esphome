#pragma once
#include "stdint.h"

namespace esphome {
namespace mbus {

#define MBUS_FRAME_DATA_LENGTH 252

//
// Frame start/stop bits
//

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
  static const MBusFrameMeta ACK_FRAME;
  static const MBusFrameMeta SHORT_FRAME;
  static const MBusFrameMeta CONTROL_FRAME;
  static const MBusFrameMeta LONG_FRAME;
};

}  // namespace mbus
}  // namespace esphome

#pragma once

#include <cstdint>

namespace esphome {
namespace sml {

enum SmlType : uint8_t {
  SML_OCTET = 0,
  SML_BOOL = 4,
  SML_INT = 5,
  SML_UINT = 6,
  SML_LIST = 7,
  SML_HEX = 10,
  SML_UNDEFINED = 255
};

enum SmlMessageType : uint16_t { SML_PUBLIC_OPEN_RES = 0x0101, SML_GET_LIST_RES = 0x701 };

// masks with two-bit mapping 0x1b -> 0b01; 0x01 -> 0b10; 0x1a -> 0b11
const uint16_t START_MASK = 0x55aa;  // 0x1b 1b 1b 1b 1b 01 01 01 01
const uint16_t END_MASK = 0x0157;    // 0x1b 1b 1b 1b 1a

}  // namespace sml
}  // namespace esphome

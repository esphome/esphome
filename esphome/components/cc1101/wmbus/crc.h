#pragma once

#include "esphome/core/log.h"

#define CRC_POLY 0x3D65

namespace esphome {
namespace wmbus {

  uint16_t crc16(uint8_t const t_message[], uint8_t t_nBytes, uint16_t t_polynomial, uint16_t t_init);
  bool crcValid(const uint8_t *t_bytes, uint8_t t_crcOffset);
  bool crcValid(const uint8_t *t_bytes, uint8_t t_crcOffset, uint8_t t_dataSize);

}
}
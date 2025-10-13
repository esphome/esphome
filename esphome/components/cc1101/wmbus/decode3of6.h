#pragma once

#include "esphome/core/log.h"
#include "m_bus_data.h"

namespace esphome {
namespace wmbus {

  uint8_t decode3of6(uint8_t t_byte);
  bool decode3OutOf6(WMbusData *t_data,  uint16_t packetSize);
  bool decode3OutOf6(uint8_t *t_encodedData, uint8_t *t_decodedData, bool t_lastByte = false);

}
}
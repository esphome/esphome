#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "m_bus_data.h"
#include "decode3of6.h"
#include "utils_my.h"
#include "crc.h"

#include <vector>

#define BLOCK1A_SIZE 12     // Size of Block 1, format A
#define BLOCK1B_SIZE 10     // Size of Block 1, format B
#define BLOCK2B_SIZE 118    // Maximum size of Block 2, format B

namespace esphome {
namespace wmbus {

  bool mBusDecode(WMbusData &t_in, WMbusFrame &t_frame);
  bool mBusDecodeFormatA(const WMbusData &t_in, WMbusFrame &t_frame);
  bool mBusDecodeFormatB(const WMbusData &t_in, WMbusFrame &t_frame);

}
}
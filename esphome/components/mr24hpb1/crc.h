#pragma once

#include <cstdint>

namespace esphome {
namespace mr24hpb1 {
uint16_t us_calculate_crc16(unsigned char *lpuc_frame, uint16_t lus_len);
}
}  // namespace esphome

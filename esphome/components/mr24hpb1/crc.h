#pragma once

namespace esphome {
namespace mr24hpb1 {
unsigned short int us_calculate_crc16(unsigned char *lpuc_frame, unsigned short int lus_len);
}
}  // namespace esphome
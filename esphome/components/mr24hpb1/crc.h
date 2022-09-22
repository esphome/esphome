#pragma once

namespace esphome {
namespace mr24hpb1 {
unsigned short int us_CalculateCrc16(unsigned char *lpuc_Frame, unsigned short int lus_Len);
}
}  // namespace esphome
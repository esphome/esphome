#include "crc.h"

namespace esphome {
namespace wmbus {

  static const char *TAG = "crc";

  uint16_t crc16(uint8_t const t_message[], uint8_t t_nBytes, uint16_t t_polynomial, uint16_t t_init) {
    uint16_t remainder{t_init};

    for (uint8_t byte{0}; byte < t_nBytes; ++byte) {
    remainder ^= t_message[byte] << 8;
      for (uint8_t bit{0}; bit < 8; ++bit) {
        if (remainder & 0x8000) {
          remainder = (remainder << 1) ^ t_polynomial;
        }
        else {
          remainder = (remainder << 1);
        }
      }
    }
    return remainder;
  }

  // Validate CRC
  bool crcValid(const uint8_t *t_bytes, uint8_t t_crcOffset) {
    bool retVal{false};
    uint16_t crcCalc = ~crc16(t_bytes, t_crcOffset, CRC_POLY, 0);
    uint16_t crcRead = (((uint16_t)t_bytes[t_crcOffset] << 8) | t_bytes[t_crcOffset+1]); // at the end
    if (crcCalc == crcRead) {
      ESP_LOGV(TAG, "    calculated: 0x%04X, read: 0x%04X", crcCalc, crcRead);
      retVal = true;
    }
    else {
      ESP_LOGD(TAG, "    calculated: 0x%04X, read: 0x%04X  !!!", crcCalc, crcRead);
      retVal = false;
    }
    return retVal;
  }

  bool crcValid(const uint8_t *t_bytes, uint8_t t_crcOffset, uint8_t t_dataSize) {
    bool retVal{false};
    uint16_t crcCalc = ~crc16((t_bytes+2), t_dataSize, CRC_POLY, 0);
    uint16_t crcRead = (((uint16_t)t_bytes[1] << 8) | t_bytes[0]);  // at the begining
    if (crcCalc == crcRead) {
      ESP_LOGV(TAG, "    calculated: 0x%04X, read: 0x%04X", crcCalc, crcRead);
      retVal = true;
    }
    else {
      ESP_LOGD(TAG, "    calculated: 0x%04X, read: 0x%04X  !!!", crcCalc, crcRead);
      retVal = false;
    }
    return retVal;
  }

}
}
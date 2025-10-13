#include "decode3of6.h"

namespace esphome {
namespace wmbus {

  static const char *TAG = "3of6";

  // Mapping from 6 bits to 4 bits. "3of6" coding used for Mode T
  uint8_t decode3of6(uint8_t t_byte) {
    uint8_t retVal{0xFF}; // Error
    switch(t_byte) {
      case 22:  retVal = 0x0;  break;  // 0x16
      case 13:  retVal = 0x1;  break;  // 0x0D
      case 14:  retVal = 0x2;  break;  // 0x0E
      case 11:  retVal = 0x3;  break;  // 0x0B
      case 28:  retVal = 0x4;  break;  // 0x1C
      case 25:  retVal = 0x5;  break;  // 0x19
      case 26:  retVal = 0x6;  break;  // 0x1A
      case 19:  retVal = 0x7;  break;  // 0x13
      case 44:  retVal = 0x8;  break;  // 0x2C
      case 37:  retVal = 0x9;  break;  // 0x25
      case 38:  retVal = 0xA;  break;  // 0x26
      case 35:  retVal = 0xB;  break;  // 0x23
      case 52:  retVal = 0xC;  break;  // 0x34
      case 49:  retVal = 0xD;  break;  // 0x31
      case 50:  retVal = 0xE;  break;  // 0x32
      case 41:  retVal = 0xF;  break;  // 0x29
      default:                 break;  // Error
    }
    return retVal;
  }

  bool decode3OutOf6(uint8_t *t_encodedData, uint8_t *t_decodedData, bool t_lastByte) {
    uint8_t data[4];

    if (t_lastByte) { // If last byte, ignore postamble sequence
      data[0] = 0x00;
      data[1] = 0x00;
    }
    else { // Perform decoding on the encoded data
      data[0] = decode3of6((*(t_encodedData + 2) & 0x3F)); 
      data[1] = decode3of6(((*(t_encodedData + 2) & 0xC0) >> 6) | ((*(t_encodedData + 1) & 0x0F) << 2));
    }

    data[2] = decode3of6(((*(t_encodedData + 1) & 0xF0) >> 4) | ((*t_encodedData & 0x03) << 4));
    data[3] = decode3of6(((*t_encodedData & 0xFC) >> 2));

    // Check for possible errors
    if ((data[0] == 0xFF) | (data[1] == 0xFF) |
        (data[2] == 0xFF) | (data[3] == 0xFF)) {
      return false;
    }

    // Prepare decoded output
    *t_decodedData = (data[3] << 4) | (data[2]);
    if (!t_lastByte) {
      *(t_decodedData + 1) = (data[1] << 4) | (data[0]);
    }
    return true;
  } 

  bool decode3OutOf6(WMbusData *t_data,  uint16_t packetSize) {
    // We can decode "in place"
    uint8_t *encodedData = t_data->data;
    uint8_t *decodedData = t_data->data; 

    uint16_t bytesDecoded{0};
    uint16_t bytesRemaining{packetSize};

    // Decode packet
    while (bytesRemaining) {
      // If last byte
      if (bytesRemaining == 1) {
        if(!decode3OutOf6(encodedData, decodedData, true)) {
          ESP_LOGV(TAG, "Decode 3 out of 6 failed.");
          return false;
        }
        bytesRemaining -= 1;
        bytesDecoded   += 1;
      }
      else {
        if(!decode3OutOf6(encodedData, decodedData)) {
          ESP_LOGV(TAG, "Decode 3 out of 6 failed..");
          return false;
        }
        bytesRemaining -= 2;
        bytesDecoded   += 2;

        encodedData += 3;
        decodedData += 2;
      }
    }
    t_data->length = bytesDecoded;
    std::fill((std::begin(t_data->data) + t_data->length), std::end(t_data->data), 0);
    ESP_LOGVV(TAG, "Decode 3 out of 6 OK.");
    return true;
  }

}
}
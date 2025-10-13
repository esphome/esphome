#include "utils_my.h"

namespace esphome {
namespace wmbus {

  static const char *TAG = "utils";

  uint16_t byteSize(uint16_t t_packetSize) {
    // In T-mode data is 3 out of 6 coded.
    uint16_t size = (( 3 * t_packetSize) / 2);

    // If packetSize is a odd number 1 extra byte
    // that includes the 4-postamble sequence must be
    // read.
    if (t_packetSize % 2) {
      return (size + 1);
    }
    else {
      return (size);
    }
  }

  uint16_t packetSize(uint8_t t_L) {
    uint16_t nrBytes;
    uint8_t  nrBlocks;

    // The 2 first blocks contains 25 bytes when excluding CRC and the L-field
    // The other blocks contains 16 bytes when excluding the CRC-fields
    // Less than 26 (15 + 10) 
    if ( t_L < 26 ) {
      nrBlocks = 2;
    }
    else { 
      nrBlocks = (((t_L - 26) / 16) + 3);
    }

    // Add all extra fields, excluding the CRC fields
    nrBytes = t_L + 1;

    // Add the CRC fields, each block is contains 2 CRC bytes
    nrBytes += (2 * nrBlocks);

    return nrBytes;
  }

}
}
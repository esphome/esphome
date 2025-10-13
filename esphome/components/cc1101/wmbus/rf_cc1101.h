#pragma once

#include "esphome/core/log.h"

#include "mbus.h"
#include "utils_my.h"
#include "decode3of6.h"
#include "m_bus_data.h"
#include "cc1101_rf_settings.h"

#include <string>
#include <stdint.h>

#include <ELECHOUSE_CC1101_SRC_DRV.h>


// CC1101 state machine
#define MARCSTATE_SLEEP            0x00
#define MARCSTATE_IDLE             0x01
#define MARCSTATE_XOFF             0x02
#define MARCSTATE_VCOON_MC         0x03
#define MARCSTATE_REGON_MC         0x04
#define MARCSTATE_MANCAL           0x05
#define MARCSTATE_VCOON            0x06
#define MARCSTATE_REGON            0x07
#define MARCSTATE_STARTCAL         0x08
#define MARCSTATE_BWBOOST          0x09
#define MARCSTATE_FS_LOCK          0x0A
#define MARCSTATE_IFADCON          0x0B
#define MARCSTATE_ENDCAL           0x0C
#define MARCSTATE_RX               0x0D
#define MARCSTATE_RX_END           0x0E
#define MARCSTATE_RX_RST           0x0F
#define MARCSTATE_TXRX_SWITCH      0x10
#define MARCSTATE_RXFIFO_OVERFLOW  0x11
#define MARCSTATE_FSTXON           0x12
#define MARCSTATE_TX               0x13
#define MARCSTATE_TX_END           0x14
#define MARCSTATE_RXTX_SWITCH      0x15
#define MARCSTATE_TXFIFO_UNDERFLOW 0x16

#define RX_FIFO_START_THRESHOLD    0
#define RX_FIFO_THRESHOLD          10  // 44 bytes in Rx FIFO

#define FIXED_PACKET_LENGTH        0x00
#define INFINITE_PACKET_LENGTH     0x02

#define MAX_FIXED_LENGTH           256

#define WMBUS_MODE_C_PREAMBLE      0x54
#define WMBUS_BLOCK_A_PREAMBLE     0xCD
#define WMBUS_BLOCK_B_PREAMBLE     0x3D

enum RxLoopState : uint8_t {
  INIT_RX       = 0,
  WAIT_FOR_SYNC = 1,
  WAIT_FOR_DATA = 2,
  READ_DATA     = 3,
};

enum Cc1101LengthMode : uint8_t {
  INFINITE      = 0,
  FIXED         = 1,
};

typedef struct RxLoopData {
  uint16_t bytesRx;
  uint8_t  lengthField;         // The L-field in the WMBUS packet
  uint16_t length;              // Total number of bytes to to be read from the RX FIFO
  uint16_t bytesLeft;           // Bytes left to to be read from the RX FIFO
  uint8_t *pByteIndex;          // Pointer to current position in the byte array
  bool complete;                // Packet received complete
  Cc1101LengthMode cc1101Mode;
  RxLoopState state;
} RxLoopData;

namespace esphome {
namespace wmbus {

  class RxLoop {
    public:
      bool init(uint8_t mosi, uint8_t miso, uint8_t clk, uint8_t cs,
                uint8_t gdo0, uint8_t gdo2, float freq, bool syncMode);
      bool task();
      WMbusFrame get_frame();

    private:
      bool start(bool force = true);

      bool syncMode{false};

      uint8_t gdo0{0};
      uint8_t gdo2{0};

      WMbusData data_in{0}; // Data from Physical layer decoded to bytes
      WMbusFrame returnFrame;

      RxLoopData rxLoop;

      uint32_t sync_time_{0};
      uint8_t  extra_time_{50};
      uint8_t  max_wait_time_ = extra_time_;
  };

}
}
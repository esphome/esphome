#pragma once

#include <cinttypes>

namespace esphome::cc1101 {

static constexpr float XTAL_FREQUENCY = 26000000;

static constexpr float RSSI_OFFSET = 74.0f;
static constexpr float RSSI_STEP = 0.5f;

static constexpr uint8_t STATUS_CRC_OK_MASK = 0x80;
static constexpr uint8_t STATUS_LQI_MASK = 0x7F;

static constexpr uint8_t BUS_BURST = 0x40;
static constexpr uint8_t BUS_READ = 0x80;
static constexpr uint8_t BUS_WRITE = 0x00;
static constexpr uint8_t BYTES_IN_RXFIFO = 0x7F;  // byte number in RXfifo
static constexpr size_t PA_TABLE_SIZE = 8;

enum class Register : uint8_t {
  IOCFG2 = 0x00,    // GDO2 output pin configuration
  IOCFG1 = 0x01,    // GDO1 output pin configuration
  IOCFG0 = 0x02,    // GDO0 output pin configuration
  FIFOTHR = 0x03,   // RX FIFO and TX FIFO thresholds
  SYNC1 = 0x04,     // Sync word, high INT8U
  SYNC0 = 0x05,     // Sync word, low INT8U
  PKTLEN = 0x06,    // Packet length
  PKTCTRL1 = 0x07,  // Packet automation control
  PKTCTRL0 = 0x08,  // Packet automation control
  ADDR = 0x09,      // Device address
  CHANNR = 0x0A,    // Channel number
  FSCTRL1 = 0x0B,   // Frequency synthesizer control
  FSCTRL0 = 0x0C,   // Frequency synthesizer control
  FREQ2 = 0x0D,     // Frequency control word, high INT8U
  FREQ1 = 0x0E,     // Frequency control word, middle INT8U
  FREQ0 = 0x0F,     // Frequency control word, low INT8U
  MDMCFG4 = 0x10,   // Modem configuration
  MDMCFG3 = 0x11,   // Modem configuration
  MDMCFG2 = 0x12,   // Modem configuration
  MDMCFG1 = 0x13,   // Modem configuration
  MDMCFG0 = 0x14,   // Modem configuration
  DEVIATN = 0x15,   // Modem deviation setting
  MCSM2 = 0x16,     // Main Radio Control State Machine configuration
  MCSM1 = 0x17,     // Main Radio Control State Machine configuration
  MCSM0 = 0x18,     // Main Radio Control State Machine configuration
  FOCCFG = 0x19,    // Frequency Offset Compensation configuration
  BSCFG = 0x1A,     // Bit Synchronization configuration
  AGCCTRL2 = 0x1B,  // AGC control
  AGCCTRL1 = 0x1C,  // AGC control
  AGCCTRL0 = 0x1D,  // AGC control
  WOREVT1 = 0x1E,   // High INT8U Event 0 timeout
  WOREVT0 = 0x1F,   // Low INT8U Event 0 timeout
  WORCTRL = 0x20,   // Wake On Radio control
  FREND1 = 0x21,    // Front end RX configuration
  FREND0 = 0x22,    // Front end TX configuration
  FSCAL3 = 0x23,    // Frequency synthesizer calibration
  FSCAL2 = 0x24,    // Frequency synthesizer calibration
  FSCAL1 = 0x25,    // Frequency synthesizer calibration
  FSCAL0 = 0x26,    // Frequency synthesizer calibration
  RCCTRL1 = 0x27,   // RC oscillator configuration
  RCCTRL0 = 0x28,   // RC oscillator configuration
  FSTEST = 0x29,    // Frequency synthesizer calibration control
  PTEST = 0x2A,     // Production test
  AGCTEST = 0x2B,   // AGC test
  TEST2 = 0x2C,     // Various test settings
  TEST1 = 0x2D,     // Various test settings
  TEST0 = 0x2E,     // Various test settings
  UNUSED = 0x2F,
  PARTNUM = 0x30,
  VERSION = 0x31,
  FREQEST = 0x32,
  LQI = 0x33,
  RSSI = 0x34,
  MARCSTATE = 0x35,
  WORTIME1 = 0x36,
  WORTIME0 = 0x37,
  PKTSTATUS = 0x38,
  VCO_VC_DAC = 0x39,
  TXBYTES = 0x3A,
  RXBYTES = 0x3B,
  RCCTRL1_STATUS = 0x3C,
  RCCTRL0_STATUS = 0x3D,
  PATABLE = 0x3E,
  FIFO = 0x3F,
};

enum class Command : uint8_t {
  RES = 0x30,     // Reset chip.
  FSTXON = 0x31,  // Enable and calibrate frequency synthesizer
  XOFF = 0x32,    // Turn off crystal oscillator.
  CAL = 0x33,     // Calibrate frequency synthesizer and turn it off
  RX = 0x34,      // Enable RX.
  TX = 0x35,      // Enable TX.
  IDLE = 0x36,    // Exit RX / TX
  // 0x37 is RESERVED / UNDEFINED in CC1101 Datasheet
  WOR = 0x38,     // Start automatic RX polling sequence (Wake-on-Radio)
  PWD = 0x39,     // Enter power down mode when CSn goes high.
  FRX = 0x3A,     // Flush the RX FIFO buffer.
  FTX = 0x3B,     // Flush the TX FIFO buffer.
  WORRST = 0x3C,  // Reset real time clock.
  NOP = 0x3D,     // No operation.
};

enum class State : uint8_t {
  SLEEP,
  IDLE,
  XOFF,
  VCOON_MC,
  REGON_MC,
  MANCAL,
  VCOON,
  REGON,
  STARTCAL,
  BWBOOST,
  FS_LOCK,
  IFADCON,
  ENDCAL,
  RX,
  RX_END,
  RX_RST,
  TXRX_SWITCH,
  RXFIFO_OVERFLOW,
  FSTXON,
  TX,
  TX_END,
  RXTX_SWITCH,
  TXFIFO_UNDERFLOW,
};

enum class RxAttenuation : uint8_t {
  RX_ATTENUATION_0DB,
  RX_ATTENUATION_6DB,
  RX_ATTENUATION_12DB,
  RX_ATTENUATION_18DB,
};

enum class SyncMode : uint8_t {
  SYNC_MODE_NONE,
  SYNC_MODE_15_16,
  SYNC_MODE_16_16,
  SYNC_MODE_30_32,
  SYNC_MODE_NONE_CS,
  SYNC_MODE_15_16_CS,
  SYNC_MODE_16_16_CS,
  SYNC_MODE_30_32_CS,
};

enum class Modulation : uint8_t {
  MODULATION_2_FSK,
  MODULATION_GFSK,
  MODULATION_UNUSED_2,
  MODULATION_ASK_OOK,
  MODULATION_4_FSK,
  MODULATION_UNUSED_5,
  MODULATION_UNUSED_6,
  MODULATION_MSK,
};

enum class MagnTarget : uint8_t {
  MAGN_TARGET_24DB,
  MAGN_TARGET_27DB,
  MAGN_TARGET_30DB,
  MAGN_TARGET_33DB,
  MAGN_TARGET_36DB,
  MAGN_TARGET_38DB,
  MAGN_TARGET_40DB,
  MAGN_TARGET_42DB,
};

enum class MaxLnaGain : uint8_t {
  MAX_LNA_GAIN_DEFAULT,
  MAX_LNA_GAIN_MINUS_2P6DB,
  MAX_LNA_GAIN_MINUS_6P1DB,
  MAX_LNA_GAIN_MINUS_7P4DB,
  MAX_LNA_GAIN_MINUS_9P2DB,
  MAX_LNA_GAIN_MINUS_11P5DB,
  MAX_LNA_GAIN_MINUS_14P6DB,
  MAX_LNA_GAIN_MINUS_17P1DB,
};

enum class MaxDvgaGain : uint8_t {
  MAX_DVGA_GAIN_DEFAULT,
  MAX_DVGA_GAIN_MINUS_1,
  MAX_DVGA_GAIN_MINUS_2,
  MAX_DVGA_GAIN_MINUS_3,
};

enum class CarrierSenseRelThr : uint8_t {
  CARRIER_SENSE_REL_THR_DEFAULT,
  CARRIER_SENSE_REL_THR_PLUS_6DB,
  CARRIER_SENSE_REL_THR_PLUS_10DB,
  CARRIER_SENSE_REL_THR_PLUS_14DB,
};

enum class FilterLengthFskMsk : uint8_t {
  FILTER_LENGTH_8DB,
  FILTER_LENGTH_16DB,
  FILTER_LENGTH_32DB,
  FILTER_LENGTH_64DB,
};

enum class FilterLengthAskOok : uint8_t {
  FILTER_LENGTH_4DB,
  FILTER_LENGTH_8DB,
  FILTER_LENGTH_12DB,
  FILTER_LENGTH_16DB,
};

enum class Freeze : uint8_t {
  FREEZE_DEFAULT,
  FREEZE_ON_SYNC,
  FREEZE_ANALOG_ONLY,
  FREEZE_ANALOG_AND_DIGITAL,
};

enum class WaitTime : uint8_t {
  WAIT_TIME_8_SAMPLES,
  WAIT_TIME_16_SAMPLES,
  WAIT_TIME_24_SAMPLES,
  WAIT_TIME_32_SAMPLES,
};

enum class HystLevel : uint8_t {
  HYST_LEVEL_NONE,
  HYST_LEVEL_LOW,
  HYST_LEVEL_MEDIUM,
  HYST_LEVEL_HIGH,
};

enum class PacketFormat : uint8_t {
  PACKET_FORMAT_FIFO,
  PACKET_FORMAT_SYNC_SERIAL,
  PACKET_FORMAT_RANDOM_TX,
  PACKET_FORMAT_ASYNC_SERIAL,
};

enum class LengthConfig : uint8_t {
  LENGTH_CONFIG_FIXED,
  LENGTH_CONFIG_VARIABLE,
  LENGTH_CONFIG_INFINITE,
};

struct __attribute__((packed)) CC1101State {
  // Byte array accessors for bulk SPI transfers
  uint8_t *regs() { return reinterpret_cast<uint8_t *>(this); }
  const uint8_t *regs() const { return reinterpret_cast<const uint8_t *>(this); }

  // 0x00
  union {
    uint8_t IOCFG2;
    struct {
      uint8_t GDO2_CFG : 6;
      uint8_t GDO2_INV : 1;
      uint8_t : 1;
    };
  };
  // 0x01
  union {
    uint8_t IOCFG1;
    struct {
      uint8_t GDO1_CFG : 6;
      uint8_t GDO1_INV : 1;
      uint8_t GDO_DS : 1;  // GDO, not GD0
    };
  };
  // 0x02
  union {
    uint8_t IOCFG0;
    struct {
      uint8_t GDO0_CFG : 6;
      uint8_t GDO0_INV : 1;
      uint8_t TEMP_SENSOR_ENABLE : 1;
    };
  };
  // 0x03
  union {
    uint8_t FIFOTHR;
    struct {
      uint8_t FIFO_THR : 4;
      uint8_t CLOSE_IN_RX : 2;  // RxAttenuation
      uint8_t ADC_RETENTION : 1;
      uint8_t : 1;
    };
  };
  // 0x04
  uint8_t SYNC1;
  // 0x05
  uint8_t SYNC0;
  // 0x06
  uint8_t PKTLEN;
  // 0x07
  union {
    uint8_t PKTCTRL1;
    struct {
      uint8_t ADR_CHK : 2;
      uint8_t APPEND_STATUS : 1;
      uint8_t CRC_AUTOFLUSH : 1;
      uint8_t : 1;
      uint8_t PQT : 3;
    };
  };
  // 0x08
  union {
    uint8_t PKTCTRL0;
    struct {
      uint8_t LENGTH_CONFIG : 2;
      uint8_t CRC_EN : 1;
      uint8_t : 1;
      uint8_t PKT_FORMAT : 2;
      uint8_t WHITE_DATA : 1;
      uint8_t : 1;
    };
  };
  // 0x09
  uint8_t ADDR;
  // 0x0A
  uint8_t CHANNR;
  // 0x0B
  union {
    uint8_t FSCTRL1;
    struct {
      uint8_t FREQ_IF : 5;
      uint8_t RESERVED : 1;  // hm?
      uint8_t : 2;
    };
  };
  // 0x0C
  uint8_t FSCTRL0;
  // 0x0D
  uint8_t FREQ2;  // [7:6] always zero
  // 0x0E
  uint8_t FREQ1;
  // 0x0F
  uint8_t FREQ0;
  // 0x10
  union {
    uint8_t MDMCFG4;
    struct {
      uint8_t DRATE_E : 4;
      uint8_t CHANBW_M : 2;
      uint8_t CHANBW_E : 2;
    };
  };
  // 0x11
  union {
    uint8_t MDMCFG3;
    struct {
      uint8_t DRATE_M : 8;
    };
  };
  // 0x12
  union {
    uint8_t MDMCFG2;
    struct {
      uint8_t SYNC_MODE : 2;
      uint8_t CARRIER_SENSE_ABOVE_THRESHOLD : 1;
      uint8_t MANCHESTER_EN : 1;
      uint8_t MOD_FORMAT : 3;  // Modulation
      uint8_t DEM_DCFILT_OFF : 1;
    };
  };
  // 0x13
  union {
    uint8_t MDMCFG1;
    struct {
      uint8_t CHANSPC_E : 2;
      uint8_t : 2;
      uint8_t NUM_PREAMBLE : 3;
      uint8_t FEC_EN : 1;
    };
  };
  // 0x14
  union {
    uint8_t MDMCFG0;
    struct {
      uint8_t CHANSPC_M : 8;
    };
  };
  // 0x15
  union {
    uint8_t DEVIATN;
    struct {
      uint8_t DEVIATION_M : 3;
      uint8_t : 1;
      uint8_t DEVIATION_E : 3;
      uint8_t : 1;
    };
  };
  // 0x16
  union {
    uint8_t MCSM2;
    struct {
      uint8_t RX_TIME : 3;
      uint8_t RX_TIME_QUAL : 1;
      uint8_t RX_TIME_RSSI : 1;
      uint8_t : 3;
    };
  };
  // 0x17
  union {
    uint8_t MCSM1;
    struct {
      uint8_t TXOFF_MODE : 2;
      uint8_t RXOFF_MODE : 2;
      uint8_t CCA_MODE : 2;
      uint8_t : 2;
    };
  };
  // 0x18
  union {
    uint8_t MCSM0;
    struct {
      uint8_t XOSC_FORCE_ON : 1;
      uint8_t PIN_CTRL_EN : 1;
      uint8_t PO_TIMEOUT : 2;
      uint8_t FS_AUTOCAL : 2;
      uint8_t : 2;
    };
  };
  // 0x19
  union {
    uint8_t FOCCFG;
    struct {
      uint8_t FOC_LIMIT : 2;
      uint8_t FOC_POST_K : 1;
      uint8_t FOC_PRE_K : 2;
      uint8_t FOC_BS_CS_GATE : 1;
      uint8_t : 2;
    };
  };
  // 0x1A
  union {
    uint8_t BSCFG;
    struct {
      uint8_t BS_LIMIT : 2;
      uint8_t BS_POST_KP : 1;
      uint8_t BS_POST_KI : 1;
      uint8_t BS_PRE_KP : 2;
      uint8_t BS_PRE_KI : 2;
    };
  };
  // 0x1B
  union {
    uint8_t AGCCTRL2;
    struct {
      uint8_t MAGN_TARGET : 3;    // MagnTarget
      uint8_t MAX_LNA_GAIN : 3;   // MaxLnaGain
      uint8_t MAX_DVGA_GAIN : 2;  // MaxDvgaGain
    };
  };
  // 0x1C
  union {
    uint8_t AGCCTRL1;
    struct {
      uint8_t CARRIER_SENSE_ABS_THR : 4;
      uint8_t CARRIER_SENSE_REL_THR : 2;  // CarrierSenseRelThr
      uint8_t AGC_LNA_PRIORITY : 1;
      uint8_t : 1;
    };
  };
  // 0x1D
  union {
    uint8_t AGCCTRL0;
    struct {
      uint8_t FILTER_LENGTH : 2;  // FilterLengthFskMsk or FilterLengthAskOok
      uint8_t AGC_FREEZE : 2;     // Freeze
      uint8_t WAIT_TIME : 2;      // WaitTime
      uint8_t HYST_LEVEL : 2;     // HystLevel
    };
  };
  // 0x1E
  uint8_t WOREVT1;
  // 0x1F
  uint8_t WOREVT0;
  // 0x20
  union {
    uint8_t WORCTRL;
    struct {
      uint8_t WOR_RES : 2;
      uint8_t : 1;
      uint8_t RC_CAL : 1;
      uint8_t EVENT1 : 3;
      uint8_t RC_PD : 1;
    };
  };
  // 0x21
  union {
    uint8_t FREND1;
    struct {
      uint8_t MIX_CURRENT : 2;
      uint8_t LODIV_BUF_CURRENT_RX : 2;
      uint8_t LNA2MIX_CURRENT : 2;
      uint8_t LNA_CURRENT : 2;
    };
  };
  // 0x22
  union {
    uint8_t FREND0;
    struct {
      uint8_t PA_POWER : 3;
      uint8_t : 1;
      uint8_t LODIV_BUF_CURRENT_TX : 2;
      uint8_t : 2;
    };
  };
  // 0x23
  union {
    uint8_t FSCAL3;
    struct {
      uint8_t FSCAL3_LO : 4;
      uint8_t CHP_CURR_CAL_EN : 2;  // Disable charge pump calibration stage when 0.
      uint8_t FSCAL3_HI : 2;
    };
  };
  // 0x24
  union {
    // uint8_t FSCAL2;
    struct {
      uint8_t FSCAL2 : 5;
      uint8_t VCO_CORE_H_EN : 1;
      uint8_t : 2;
    };
  };
  // 0x25
  union {
    // uint8_t FSCAL1;
    struct {
      uint8_t FSCAL1 : 6;
      uint8_t : 2;
    };
  };
  // 0x26
  union {
    // uint8_t FSCAL0;
    struct {
      uint8_t FSCAL0 : 7;
      uint8_t : 1;
    };
  };
  // 0x27
  union {
    // uint8_t RCCTRL1;
    struct {
      uint8_t RCCTRL1 : 7;
      uint8_t : 1;
    };
  };
  // 0x28
  union {
    // uint8_t RCCTRL0;
    struct {
      uint8_t RCCTRL0 : 7;
      uint8_t : 1;
    };
  };
  // 0x29
  uint8_t FSTEST;
  // 0x2A
  uint8_t PTEST;
  // 0x2B
  uint8_t AGCTEST;
  // 0x2C
  uint8_t TEST2;
  // 0x2D
  uint8_t TEST1;
  // 0x2E
  union {
    uint8_t TEST0;
    struct {
      uint8_t TEST0_LO : 1;
      uint8_t VCO_SEL_CAL_EN : 1;  // Enable VCO selection calibration stage when 1
      uint8_t TEST0_HI : 6;
    };
  };
  // 0x2F
  uint8_t REG_2F;
  // 0x30
  uint8_t PARTNUM;
  // 0x31
  uint8_t VERSION;
  // 0x32
  union {
    uint8_t FREQEST;
    struct {
      int8_t FREQOFF_EST : 8;
    };
  };
  // 0x33
  union {
    uint8_t LQI;
    struct {
      uint8_t LQI_EST : 7;
      uint8_t LQI_CRC_OK : 1;
    };
  };
  // 0x34
  int8_t RSSI;
  // 0x35
  union {
    // uint8_t MARCSTATE;
    struct {
      uint8_t MARC_STATE : 5;  // State
      uint8_t : 3;
    };
  };
  // 0x36
  uint8_t WORTIME1;
  // 0x37
  uint8_t WORTIME0;
  // 0x38
  union {
    uint8_t PKTSTATUS;
    struct {
      uint8_t GDO0 : 1;
      uint8_t : 1;
      uint8_t GDO2 : 1;
      uint8_t SFD : 1;
      uint8_t CCA : 1;
      uint8_t PQT_REACHED : 1;
      uint8_t CS : 1;
      uint8_t CRC_OK : 1;  // same as LQI_CRC_OK?
    };
  };
  // 0x39
  uint8_t VCO_VC_DAC;
  // 0x3A
  union {
    uint8_t TXBYTES;
    struct {
      uint8_t NUM_TXBYTES : 7;
      uint8_t TXFIFO_UNDERFLOW : 1;
    };
  };
  // 0x3B
  union {
    uint8_t RXBYTES;
    struct {
      uint8_t NUM_RXBYTES : 7;
      uint8_t RXFIFO_OVERFLOW : 1;
    };
  };
  // 0x3C
  union {
    // uint8_t RCCTRL1_STATUS;
    struct {
      uint8_t RCCTRL1_STATUS : 7;
      uint8_t : 1;
    };
  };
  // 0x3D
  union {
    // uint8_t RCCTRL0_STATUS;
    struct {
      uint8_t RCCTRL0_STATUS : 7;
      uint8_t : 1;
    };
  };
  // 0x3E
  uint8_t REG_3E;
  // 0x3F
  uint8_t REG_3F;
};

static_assert(sizeof(CC1101State) == 0x40, "CC1101State size mismatch");

}  // namespace esphome::cc1101

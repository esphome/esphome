#pragma once

#include <cinttypes>

namespace esphome {
namespace cc1101 {

static constexpr float XTAL_FREQUENCY = 26000;
static constexpr float OUTPUT_POWER_MIN = -30;
static constexpr float OUTPUT_POWER_MAX = 11;
static constexpr float FREQUENCY_MIN = 300000;
static constexpr float FREQUENCY_MAX = 928000;
static constexpr float IF_FREQUENCY_MIN = 25;
static constexpr float IF_FREQUENCY_MAX = 788;
static constexpr float BANDWIDTH_MIN = 58;
static constexpr float BANDWIDTH_MAX = 812;
static constexpr uint8_t CHANNEL_MIN = 0;
static constexpr uint8_t CHANNEL_MAX = 255;
static constexpr float CHANNEL_SPACING_MIN = 25;
static constexpr float CHANNEL_SPACING_MAX = 405;
static constexpr float FSK_DEVIATION_MIN = 1.5f;
static constexpr float FSK_DEVIATION_MAX = 381;
static constexpr uint8_t MSK_DEVIATION_MIN = 1;
static constexpr uint8_t MSK_DEVIATION_MAX = 8;
static constexpr float SYMBOL_RATE_MIN = 600;
static constexpr float SYMBOL_RATE_MAX = 500000;
static constexpr int8_t CARRIER_SENSE_ABS_THR_MIN = -8;
static constexpr int8_t CARRIER_SENSE_ABS_THR_MAX = 7;

static constexpr uint8_t BUS_BURST = 0x40;
static constexpr uint8_t BUS_READ = 0x80;
static constexpr uint8_t BUS_WRITE = 0x00;
static constexpr uint8_t BYTES_IN_RXFIFO = 0x7F;  // byte number in RXfifo

enum class Register : uint8_t {
  IOCFG2,    // GDO2 output pin configuration
  IOCFG1,    // GDO1 output pin configuration
  IOCFG0,    // GDO0 output pin configuration
  FIFOTHR,   // RX FIFO and TX FIFO thresholds
  SYNC1,     // Sync word, high INT8U
  SYNC0,     // Sync word, low INT8U
  PKTLEN,    // Packet length
  PKTCTRL1,  // Packet automation control
  PKTCTRL0,  // Packet automation control
  ADDR,      // Device address
  CHANNR,    // Channel number
  FSCTRL1,   // Frequency synthesizer control
  FSCTRL0,   // Frequency synthesizer control
  FREQ2,     // Frequency control word, high INT8U
  FREQ1,     // Frequency control word, middle INT8U
  FREQ0,     // Frequency control word, low INT8U
  MDMCFG4,   // Modem configuration
  MDMCFG3,   // Modem configuration
  MDMCFG2,   // Modem configuration
  MDMCFG1,   // Modem configuration
  MDMCFG0,   // Modem configuration
  DEVIATN,   // Modem deviation setting
  MCSM2,     // Main Radio Control State Machine configuration
  MCSM1,     // Main Radio Control State Machine configuration
  MCSM0,     // Main Radio Control State Machine configuration
  FOCCFG,    // Frequency Offset Compensation configuration
  BSCFG,     // Bit Synchronization configuration
  AGCCTRL2,  // AGC control
  AGCCTRL1,  // AGC control
  AGCCTRL0,  // AGC control
  WOREVT1,   // High INT8U Event 0 timeout
  WOREVT0,   // Low INT8U Event 0 timeout
  WORCTRL,   // Wake On Radio control
  FREND1,    // Front end RX configuration
  FREND0,    // Front end TX configuration
  FSCAL3,    // Frequency synthesizer calibration
  FSCAL2,    // Frequency synthesizer calibration
  FSCAL1,    // Frequency synthesizer calibration
  FSCAL0,    // Frequency synthesizer calibration
  RCCTRL1,   // RC oscillator configuration
  RCCTRL0,   // RC oscillator configuration
  FSTEST,    // Frequency synthesizer calibration control
  PTEST,     // Production test
  AGCTEST,   // AGC test
  TEST2,     // Various test settings
  TEST1,     // Various test settings
  TEST0,     // Various test settings
  UNUSED,
  PARTNUM,
  VERSION,
  FREQEST,
  LQI,
  RSSI,
  MARCSTATE,
  WORTIME1,
  WORTIME0,
  PKTSTATUS,
  VCO_VC_DAC,
  TXBYTES,
  RXBYTES,
  RCCTRL1_STATUS,
  RCCTRL0_STATUS,
  PATABLE,
  FIFO,
};

enum class Command : uint8_t {
  RES = 0x30,  // Reset chip.
  FSTXON,      // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1).
               // If in RX/TX: Go to a wait state where only the synthesizer is
               // running (for quick RX / TX turnaround).
  XOFF,        // Turn off crystal oscillator.
  CAL,         // Calibrate frequency synthesizer and turn it off
               // (enables quick start).
  RX,          // Enable RX. Perform calibration first if coming from IDLE and
               // MCSM0.FS_AUTOCAL=1.
  TX,          // In IDLE state: Enable TX. Perform calibration first if
               // MCSM0.FS_AUTOCAL=1. If in RX state and CCA is enabled:
               // Only go to TX if channel is clear.
  IDLE,        // Exit RX / TX, turn off frequency synthesizer and exit
               // Wake-On-Radio mode if applicable.
  AFC,         // Perform AFC adjustment of the frequency synthesizer
  WOR,         // Start automatic RX polling sequence (Wake-on-Radio)
  PWD,         // Enter power down mode when CSn goes high.
  FRX,         // Flush the RX FIFO buffer.
  FTX,         // Flush the TX FIFO buffer.
  WORRST,      // Reset real time clock.
  NOP,         // No operation. May be used to pad strobe commands to two
               // INT8Us for simpler software.
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
  LAST,
};

enum class FrequencyPreset : uint8_t {
  FREQUENCY_433MHZ,
  FREQUENCY_915MHZ,
  FREQUENCY_868MHZ,
  FREQUENCY_315MHZ,
  FREQUENCY_MANUAL,  // For when the number slider is used
  LAST,
};
// MDMCFG2

enum class SyncMode : uint8_t {
  SYNC_MODE_NONE,
  SYNC_MODE_15_16,
  SYNC_MODE_16_16,
  SYNC_MODE_30_32,
  LAST,
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
  LAST,
};

// AGCCTRL2

enum class MagnTarget : uint8_t {
  MAGN_TARGET_24DB,
  MAGN_TARGET_27DB,
  MAGN_TARGET_30DB,
  MAGN_TARGET_33DB,
  MAGN_TARGET_36DB,
  MAGN_TARGET_38DB,
  MAGN_TARGET_40DB,
  MAGN_TARGET_42DB,
  LAST,
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
  LAST,
};

enum class MaxDvgaGain : uint8_t {
  MAX_DVGA_GAIN_DEFAULT,
  MAX_DVGA_GAIN_MINUS_1,
  MAX_DVGA_GAIN_MINUS_2,
  MAX_DVGA_GAIN_MINUS_3,
  LAST,
};

// AGCCTRL1

// CARRIER_SENSE_ABS_THR => number -7..+7, -8 is disabled

enum class CarrierSenseRelThr : uint8_t {
  CARRIER_SENSE_REL_THR_DEFAULT,
  CARRIER_SENSE_REL_THR_PLUS_6DB,
  CARRIER_SENSE_REL_THR_PLUS_10DB,
  CARRIER_SENSE_REL_THR_PLUS_14DB,
  LAST,
};

// AGC_LNA_PRIORITY => switch

// AGCCTRL0

enum class FilterLengthFskMsk : uint8_t {
  FILTER_LENGTH_8DB,
  FILTER_LENGTH_16DB,
  FILTER_LENGTH_32DB,
  FILTER_LENGTH_64DB,
  LAST,
};

enum class FilterLengthAskOok : uint8_t {
  FILTER_LENGTH_4DB,
  FILTER_LENGTH_8DB,
  FILTER_LENGTH_12DB,
  FILTER_LENGTH_16DB,
  LAST,
};

enum class Freeze : uint8_t {
  FREEZE_DEFAULT,
  FREEZE_ON_SYNC,
  FREEZE_ANALOG_ONLY,
  FREEZE_ANALOG_AND_DIGITAL,
  LAST,
};

enum class WaitTime : uint8_t {
  WAIT_TIME_8_SAMPLES,
  WAIT_TIME_16_SAMPLES,
  WAIT_TIME_24_SAMPLES,
  WAIT_TIME_32_SAMPLES,
  LAST,
};

enum class HystLevel : uint8_t {
  HYST_LEVEL_NONE,
  HYST_LEVEL_LOW,
  HYST_LEVEL_MEDIUM,
  HYST_LEVEL_HIGH,
  LAST,
};

//

struct CC1101State {
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

}  // namespace cc1101
}  // namespace esphome

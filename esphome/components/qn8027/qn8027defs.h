#pragma once

namespace esphome {
namespace qn8027 {

static const uint8_t REG_SYSTEM_ADDR = 0x00;
static const uint8_t REG_CH1_ADDR = 0x01;
static const uint8_t REG_GPLT_ADDR = 0x02;
static const uint8_t REG_XTL_ADDR = 0x03;
static const uint8_t REG_VGA_ADDR = 0x04;
static const uint8_t REG_CID1_ADDR = 0x05;
static const uint8_t REG_CID2_ADDR = 0x06;
static const uint8_t REG_STATUS_ADDR = 0x07;
static const uint8_t REG_RDSD_ADDR = 0x08;  // 0x08 - 0x0F
static const uint8_t REG_PAC_ADDR = 0x10;
static const uint8_t REG_FDEV_ADDR = 0x11;
static const uint8_t REG_RDS_ADDR = 0x12;

static const float CH_FREQ_MIN = 76;
static const float CH_FREQ_MAX = 108;
static const int CH_FREQ_RAW_MIN = 0;
static const int CH_FREQ_RAW_MAX = 640;

static const float TX_FDEV_MIN = 0;
static const float TX_FDEV_MAX = 147.9f;
static const int TX_FDEV_RAW_MIN = 0;
static const int TX_FDEV_RAW_MAX = 255;

static const uint8_t GAIN_TXPLT_MIN = 7;
static const uint8_t GAIN_TXPLT_MAX = 15;

static const uint8_t GDB_MAX = 2;
static const uint8_t GVGA_MAX = 5;

static const float PA_TRGT_MIN = 83.4f;
static const float PA_TRGT_MAX = 117.5f;

static const uint8_t RDS_STATION_MAX_SIZE = 8;
static const uint8_t RDS_TEXT_MAX_SIZE = 64;

static const uint8_t CID1_FM = 0;
static const uint8_t CID3_QN8027 = 4;

static const uint16_t XISEL_MAX = 393.75f;

// TODO: enum class
static const uint8_t FSM_STATUS_RESET = 0;
static const uint8_t FSM_STATUS_CALI = 1;
static const uint8_t FSM_STATUS_IDLE = 2;
static const uint8_t FSM_STATUS_TX_RSTB = 3;
static const uint8_t FSM_STATUS_PA_CALIB = 4;
static const uint8_t FSM_STATUS_TRANSMIT = 5;
static const uint8_t FSM_STATUS_PA_OFF = 6;
static const uint8_t FSM_STATUS_RESERVED = 7;

enum class T1mSel {
  T1M_SEL_58S,
  T1M_SEL_59S,
  T1M_SEL_60S,
  T1M_SEL_NEVER,
  LAST,
};

enum class PreEmphasis {
  TC_50US,
  TC_75US,
  LAST,
};

enum class XtalSource {
  USE_CRYSTAL_ON_XTAL12,
  DIGITAL_CLOCK_ON_XTAL1,
  SINGLE_END_SIN_WAVE_ON_XTAL1,
  DIFFERENTIAL_SIN_WAVE_ON_XTAL12,
  LAST,
};

enum class XtalFrequency {
  XSEL_12MHZ,
  XSEL_24MHZ,
  LAST,
};

enum class InputImpedance {
  VGA_RIN_5KOHM,
  VGA_RIN_10KOHM,
  VGA_RIN_20KOHM,
  VGA_RIN_40KOHM,
  LAST,
};

struct qn8027_state_t {
  union {
    uint8_t REG_SYSTEM;
    struct {
      uint8_t CH_UPPER : 2;  // wo
      uint8_t RDSRDY : 1;    // wo
      uint8_t MUTE : 1;      // wo
      uint8_t MONO : 1;      // wo
      uint8_t TXREQ : 1;     // wo
      uint8_t RECAL : 1;     // wo
      uint8_t SWRST : 1;     // r/w
    };
  };
  union {
    uint8_t REG_CH1;
    struct {
      uint8_t CH_LOWER;  // wo
    };
  };
  union {
    uint8_t REG_GPLT;
    struct {
      uint8_t GAIN_TXPLT : 4;  // wo
      uint8_t t1m_sel : 2;     // wo
      uint8_t priv_en : 1;     // wo
      uint8_t TC : 1;          // wo
    };
  };
  union {
    uint8_t REG_XLT;
    struct {
      uint8_t XISEL : 6;  // wo
      uint8_t XINJ : 2;   // wo
    };
  };
  union {
    uint8_t REG_VGA;
    struct {
      uint8_t RIN : 2;   // wo
      uint8_t GDB : 2;   // wo
      uint8_t GVGA : 3;  // wo
      uint8_t XSEL : 1;  // wo
    };
  };
  union {
    uint8_t REG_CID1;
    struct {
      uint8_t CID2 : 2;  // ro
      uint8_t CID1 : 3;  // ro
      uint8_t CID0 : 3;  // ro
    };
  };
  union {
    uint8_t REG_CID2;
    struct {
      uint8_t CID4 : 4;  // ro
      uint8_t CID3 : 4;  // ro
    };
  };
  union {
    uint8_t REG_STATUS;
    struct {
      uint8_t FSM : 3;      // ro
      uint8_t RDS_UPD : 1;  // ro
      uint8_t aud_pk : 4;   // ro
    };
  };
  uint8_t REG_RDSD[8];  // wo
  union {
    uint8_t REG_PAC;
    struct {
      uint8_t PA_TRGT : 7;   // wo
      uint8_t TXPD_CLR : 1;  // wo
    };
  };
  union {
    uint8_t REG_FDEV;
    struct {
      uint8_t TX_FDEV;  // wo
    };
  };
  union {
    uint8_t REG_RDS;
    struct {
      uint8_t RDSFDEV : 7;  // wo
      uint8_t RDSEN : 1;    // wo
    };
  };
};

}  // namespace qn8027
}  // namespace esphome
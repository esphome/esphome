#pragma once

namespace esphome {
namespace kt0803 {

static const float CHSEL_MIN = 70.0f;
static const float CHSEL_MAX = 108.0f;
static const float CHSEL_STEP = 0.05;

static const float PGA_MIN = -15;
static const float PGA_MAX = 12;
static const float PGA_STEP = 1;

static const float RFGAIN_MIN = 95.5f;
static const float RFGAIN_MAX = 108.0f;

static const float ALC_GAIN_MIN = -15;
static const float ALC_GAIN_MAX = 6;

enum class ChipId {
  KT0803,
  KT0803K,
  KT0803M,
  KT0803L,
};

enum class PreEmphasis {
  PHTCNST_50US,
  PHTCNST_75US,
  LAST,
};

enum class PilotToneAmplitude {
  PLTADJ_LOW,
  PLTADJ_HIGH,
  LAST,
};

enum class BassBoostControl {
  BASS_DISABLED,
  BASS_5DB,
  BASS_11DB,
  BASS_17DB,
  LAST,
};

enum class AlcTime {
  ALC_TIME_25US,
  ALC_TIME_50US,
  ALC_TIME_75US,
  ALC_TIME_100US,
  ALC_TIME_125US,
  ALC_TIME_150US,
  ALC_TIME_175US,
  ALC_TIME_200US,
  ALC_TIME_50MS,
  ALC_TIME_100MS,
  ALC_TIME_150MS,
  ALC_TIME_200MS,
  ALC_TIME_250MS,
  ALC_TIME_300MS,
  ALC_TIME_350MS,
  ALC_TIME_400MS,
  LAST,
};

enum class SwitchMode {
  SW_MOD_MUTE,
  SW_MOD_PA_OFF,
  LAST,
};

enum class SilenceHigh {
  SLNCTHH_0M5V,
  SLNCTHH_1MV,
  SLNCTHH_2MV,
  SLNCTHH_4MV,
  SLNCTHH_8MV,
  SLNCTHH_16MV,
  SLNCTHH_32MV,
  SLNCTHH_64MV,
  LAST,
};

enum class SilenceLow {
  SLNCTHL_0M25V,
  SLNCTHL_0M5V,
  SLNCTHL_1MV,
  SLNCTHL_2MV,
  SLNCTHL_4MV,
  SLNCTHL_8MV,
  SLNCTHL_16MV,
  SLNCTHL_32MV,
  LAST,
};

enum class SilenceLowAndHighLevelDurationTime {
  SLNCTIME_50MS,
  SLNCTIME_100MS,
  SLNCTIME_200MS,
  SLNCTIME_400MS,
  SLNCTIME_1S,
  SLNCTIME_2S,
  SLNCTIME_4S,
  SLNCTIME_8S,
  SLNCTIME_16S,
  SLNCTIME_24S,
  SLNCTIME_32S,
  SLNCTIME_40S,
  SLNCTIME_48S,
  SLNCTIME_56S,
  SLNCTIME_60S,
  SLNCTIME_64S,
  LAST,
};

enum class SilenceHighLevelCounter {
  SLNCCNTHIGH_15,
  SLNCCNTHIGH_31,
  SLNCCNTHIGH_63,
  SLNCCNTHIGH_127,
  SLNCCNTHIGH_255,
  SLNCCNTHIGH_511,
  SLNCCNTHIGH_1023,
  SLNCCNTHIGH_2047,
  LAST,
};

enum class AlcCompressedGain {
  ALCCMPGAIN_N6DB,
  ALCCMPGAIN_N9DB,
  ALCCMPGAIN_N12DB,
  ALCCMPGAIN_N15DB,
  ALCCMPGAIN_6DB,
  ALCCMPGAIN_3DB,
  ALCCMPGAIN_0DB,
  ALCCMPGAIN_N3DB,
  LAST,
};

enum class SilenceLowLevelCounter {
  SLNCCNTLOW_1,
  SLNCCNTLOW_2,
  SLNCCNTLOW_4,
  SLNCCNTLOW_8,
  SLNCCNTLOW_16,
  SLNCCNTLOW_32,
  SLNCCNTLOW_64,
  SLNCCNTLOW_128,
  LAST,
};

enum class XtalSel {
  XTAL_SEL_32K768HZ,
  XTAL_SEL_7M6HZ,
  LAST,
};

enum class FrequencyDeviation {
  FDEV_75KHZ,
  FDEV_112K5HZ,
  FDEV_150KHZ,
  FDEV_187K5HZ,
  LAST,
};

enum class ReferenceClock {
  REF_CLK_32K768HZ,
  REF_CLK_6M5HZ,
  REF_CLK_7M6HZ,
  REF_CLK_12MHZ,
  REF_CLK_13MHZ,
  REF_CLK_15M2HZ,
  REF_CLK_19M2HZ,
  REF_CLK_24MHZ,
  REF_CLK_26MHZ,
  LAST,
};

enum class AlcHigh {
  ALCHIGHTH_06,
  ALCHIGHTH_05,
  ALCHIGHTH_04,
  ALCHIGHTH_03,
  ALCHIGHTH_02,
  ALCHIGHTH_01,
  ALCHIGHTH_005,
  ALCHIGHTH_001,
  LAST,
};

enum class AlcHoldTime {
  ALCHOLD_50MS,
  ALCHOLD_100MS,
  ALCHOLD_150MS,
  ALCHOLD_200MS,
  ALCHOLD_1S,
  ALCHOLD_5S,
  ALCHOLD_10S,
  ALCHOLD_15S,
  LAST,
};

enum class AlcLow {
  ALCLOWTH_025,
  ALCLOWTH_020,
  ALCLOWTH_015,
  ALCLOWTH_010,
  ALCLOWTH_005,
  ALCLOWTH_003,
  ALCLOWTH_002,
  ALCLOWTH_001,
  ALCLOWTH_0005,
  ALCLOWTH_0001,
  ALCLOWTH_00005,
  ALCLOWTH_00001,
  LAST,
};

enum class AudioLimiterLevel {
  LMTLVL_06875,
  LMTLVL_07500,
  LMTLVL_08750,
  LMTLVL_09625,
  LAST,
};

// 0 = KT0803
// K = KT0803K or KT0803M (not datasheet for M, it should be mostly the same as K)
// L = KT0803L

struct KT0803State {
  union {
    uint8_t REG_00;
    struct {
      uint8_t CHSEL1 : 8;  // 0 K L
    };
  };
  union {
    uint8_t REG_01;
    struct {
      uint8_t CHSEL2 : 3;   // 0 K L
      uint8_t PGA : 3;      // 0 K L
      uint8_t RFGAIN0 : 2;  // 0 K L
    };
  };
  union {
    uint8_t REG_02;
    struct {
      uint8_t PHTCNST : 1;  // 0 K L
      uint8_t _02_1 : 1;
      uint8_t PLTADJ : 1;  // 0 K L
      uint8_t MUTE : 1;    // 0 K L
      uint8_t _02_2 : 2;
      uint8_t RFGAIN2 : 1;  // 0 K L
      uint8_t CHSEL0 : 1;   // K L (no LSB on 0, step size is only 100kHz)
    };
  };
  uint8_t REG_03;
  union {
    uint8_t REG_04;
    struct {
      uint8_t BASS : 2;     // K L
      uint8_t FDEV_K : 2;   // K
      uint8_t PGA_LSB : 2;  // K L
      uint8_t MONO : 1;     // K L
      uint8_t ALC_EN : 1;   // L
    };
  };
  uint8_t REG_05;
  uint8_t REG_06;
  uint8_t REG_07;
  uint8_t REG_08;
  uint8_t REG_09;
  uint8_t REG_0A;
  union {
    uint8_t REG_0B;
    struct {
      uint8_t _0B_1 : 2;
      uint8_t AUTO_PADN : 1;  // L
      uint8_t _0B_2 : 2;
      uint8_t PDPA : 1;  // K L
      uint8_t _0B_3 : 1;
      uint8_t Standby : 1;  // L
    };
  };
  union {
    uint8_t REG_0C;
    struct {
      uint8_t ALC_ATTACK_TIME : 4;  // L
      uint8_t ALC_DECAY_TIME : 4;   // L
    };
  };
  uint8_t REG_0D;
  union {
    uint8_t REG_0E;
    struct {
      uint8_t _0E_1 : 1;
      uint8_t PA_BIAS : 1;  // K L
      uint8_t _0E_2 : 6;
    };
  };
  union {
    uint8_t REG_0F;
    struct {
      uint8_t _0F_1 : 2;
      uint8_t SLNCID : 1;  // K L (ro)
      uint8_t _0F_2 : 1;
      uint8_t PW_OK : 1;  // K L (ro)
      uint8_t _0F_3 : 3;
    };
  };
  union {
    uint8_t REG_10;
    struct {
      uint8_t PGAMOD : 1;  // K L
      uint8_t _10_1 : 2;
      uint8_t LMTLVL : 2;  // K
      uint8_t _10_2 : 3;
    };
  };
  uint8_t REG_11;
  union {
    uint8_t REG_12;
    struct {
      uint8_t SW_MOD : 1;   // K L
      uint8_t SLNCTHH : 3;  // K L
      uint8_t SLNCTHL : 3;  // K L
      uint8_t SLNCDIS : 1;  // K L
    };
  };
  union {
    uint8_t REG_13;
    struct {
      uint8_t _13_1 : 2;
      uint8_t PA_CTRL : 1;  // 0 K L
      uint8_t _13_2 : 4;
      uint8_t RFGAIN1 : 1;  // 0 K L
    };
  };
  union {
    uint8_t REG_14;
    struct {
      uint8_t SLNCTIME1 : 1;  // L
      uint8_t _14_1 : 1;
      uint8_t SLNCCNTHIGH : 3;  // K L
      uint8_t SLNCTIME0 : 3;    // K L
    };
  };
  union {
    uint8_t REG_15;
    struct {
      uint8_t _15_1 : 5;
      uint8_t ALCCMPGAIN : 3;  // L
    };
  };
  union {
    uint8_t REG_16;
    struct {
      uint8_t SLNCCNTLOW : 3;  // K L
      uint8_t _16_1 : 5;
    };
  };
  union {
    uint8_t REG_17;
    struct {
      uint8_t _17_1 : 3;
      uint8_t XTAL_SEL : 1;  // L
      uint8_t _17_2 : 1;
      uint8_t AU_ENHANCE : 1;  // L
      uint8_t FDEV_L : 1;      // L
      uint8_t _17_3 : 1;       // is this part of FDEV? FDEV_K is two bits
    };
  };
  uint8_t REG_18;
  uint8_t REG_19;
  uint8_t REG_1A;
  uint8_t REG_1B;
  uint8_t REG_1C;
  uint8_t REG_1D;
  union {
    uint8_t REG_1E;
    struct {
      uint8_t _1E_1 : 1;
      uint8_t REF_CLK : 3;  // L
      uint8_t _1E_2 : 1;
      uint8_t XTALD : 1;  // L
      uint8_t DCLK : 1;   // L
      uint8_t _1E_3 : 1;
    };
  };
  uint8_t REG_1F;
  uint8_t REG_20;
  uint8_t REG_21;
  uint8_t REG_22;
  uint8_t REG_23;
  uint8_t REG_24;
  uint8_t REG_25;
  union {
    uint8_t REG_26;
    struct {
      uint8_t _26_1 : 1;
      uint8_t ALCHIGHTH : 3;  // L
      uint8_t _26_2 : 1;
      uint8_t ALCHOLD : 3;  // L
    };
  };
  union {
    uint8_t REG_27;
    struct {
      uint8_t ALCLOWTH : 4;  // L
      uint8_t _27_1 : 4;
    };
  };
};

}  // namespace kt0803
}  // namespace esphome

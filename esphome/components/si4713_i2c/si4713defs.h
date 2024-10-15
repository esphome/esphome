#pragma once

namespace esphome {
namespace si4713 {

static const uint8_t SI4710_STATUS_CTS = 0x80;

static const float CH_FREQ_MIN = 76;
static const float CH_FREQ_MAX = 108;

static const uint8_t RDS_STATION_MAX = 8;
static const uint8_t RDS_TEXT_MAX = 64;

enum class CmdType : uint8_t {
  POWER_UP = 0x01,         // Power up device and mode selection.
  GET_REV = 0x10,          // Returns revision information on the device.
  POWER_DOWN = 0x11,       // Power down device.
  SET_PROPERTY = 0x12,     // Sets the value of a property.
  GET_PROPERTY = 0x13,     // Retrieves a property’s value.
  GET_INT_STATUS = 0x14,   // Read interrupt status bits.
  PATCH_ARGS = 0x15,       // Reserved command used for patch file downloads.
  PATCH_DATA = 0x16,       // Reserved command used for patch file downloads.
  TX_TUNE_FREQ = 0x30,     // Tunes to given transmit frequency.
  TX_TUNE_POWER = 0x31,    // Sets the output power level and tunes the antenna capacitor
  TX_TUNE_MEASURE = 0x32,  // Measure the received noise level at the specified frequency.
  TX_TUNE_STATUS = 0x33,   // Queries the status of a previously sent TX Tune Freq, Power, or Measure command.
  TX_ASQ_STATUS = 0x34,    // Queries the TX status and input audio signal metrics.
  TX_RDS_BUFF = 0x35,      // Si4713 Only. Queries the status of the RDS Group Buffer and loads new data into buffer.
  TX_RDS_PS = 0x36,        // Si4713 Only. Set up default PS strings.
  GPIO_CTL = 0x80,         // Configures GPO3 as output or Hi-Z.
  GPIO_SET = 0x81,         // Sets GPO3 output level (low or high).
};

enum class PropType : uint16_t {
  // Enables interrupt sources.
  // Default: 0x0000
  GPO_IEN = 0x0001,
  // Configures the digital input format.
  // Default: 0x0000
  DIGITAL_INPUT_FORMAT = 0x0101,
  // Configures the digital input sample rate in 10 Hz steps. Default is 0.
  // Default: 0x0000
  DIGITAL_INPUT_SAMPLE_RATE = 0x0103,
  // Sets frequency of the reference clock in Hz. The range is 31130 to 34406 Hz, or 0 to disable the AFC. Default is
  // 32768 Hz.
  // Default: 0x8000
  REFCLK_FREQ = 0x0201,
  // Sets the prescaler value for the reference clock.
  // Default: 0x0001
  REFCLK_PRESCALE = 0x0202,
  // Enable transmit multiplex signal components. Default has pilot and L-R enabled.
  // Default: 0x0003
  TX_COMPONENT_ENABLE = 0x2100,
  // Configures audio frequency deviation level. Units are in 10 Hz increments. Default is 6285 (68.25 kHz).
  // Default: 0x1AA9
  TX_AUDIO_DEVIATION = 0x2101,
  // Configures pilot tone frequency deviation level. Units are in 10 Hz increments. Default is 675 (6.75 kHz)
  // Default: 0x02A3
  TX_PILOT_DEVIATION = 0x2102,
  // Si4713 Only. Configures the RDS/RBDS frequency deviation level. Units are in 10 Hz increments. Default is 2 kHz.
  // Default: 0x00C8
  TX_RDS_DEVIATION = 0x2103,
  // Configures maximum analog line input level to the LIN/RIN pins to reach the maximum deviation level programmed
  // into the audio deviation property TX Audio Deviation. Default is 636 mVPK.
  // Default: 0x327C
  TX_LINE_INPUT_LEVEL = 0x2104,
  // Sets line input mute. L and R inputs may be independently muted. Default is not muted.
  // Default: 0x0000
  TX_LINE_INPUT_MUTE = 0x2105,
  // Configures pre-emphasis time constant. Default is 0 (75 µS).
  // Default: 0x0000
  TX_PREEMPHASIS = 0x2106,
  // Configures the frequency of the stereo pilot. Default is 19000 Hz.
  // Default: 0x4A38
  TX_PILOT_FREQUENCY = 0x2107,
  // Enables audio dynamic range control. Default is 0 (disabled).
  // Default: 0x0002
  TX_ACOMP_ENABLE = 0x2200,
  // Sets the threshold level for audio dynamic range control. Default is –40 dB.
  // Default: 0xFFD8
  TX_ACOMP_THRESHOLD = 0x2201,
  // Sets the attack time for audio dynamic range control. Default is 0 (0.5 ms).
  // Default: 0x0000
  TX_ACOMP_ATTACK_TIME = 0x2202,
  // Sets the release time for audio dynamic range control. Default is 4 (1000 ms).
  // Default: 0x0004
  TX_ACOMP_RELEASE_TIME = 0x2203,
  // Sets the gain for audio dynamic range control. Default is 15 dB.
  // Default: 0x000F
  TX_ACOMP_GAIN = 0x2204,
  // Sets the limiter release time. Default is 102 (5.01 ms)
  // Default: 0x0066
  TX_LIMITER_RELEASE_TIME = 0x2205,
  // Configures measurements related to signal quality metrics. Default is none selected.
  // Default: 0x0000
  TX_ASQ_INTERRUPT_SOURCE = 0x2300,
  // Configures low audio input level detection threshold. This threshold can be used to detect silence on the
  // incoming audio.
  // Default: 0x0000
  TX_ASQ_LEVEL_LOW = 0x2301,
  // Configures the duration which the input audio level must be below the low threshold in order to detect a low
  // audio condition.
  // Default: 0x0000
  TX_ASQ_DURATION_LOW = 0x2302,
  // Configures high audio input level detection threshold. This threshold can be used to detect activity on the
  // incoming audio.
  // Default: 0x0000
  TX_ASQ_LEVEL_HIGH = 0x2303,
  // Configures the duration which the input audio level must be above the high threshold in order to detect a high
  // audio condition.
  // Default: 0x0000
  TX_ASQ_DURATION_HIGH = 0x2304,
  // Si4713 Only. Configure RDS interrupt sources. Default is none selected.
  // Default: 0x0000
  TX_RDS_INTERRUPT_SOURCE = 0x2C00,
  // Si4713 Only. Sets transmit RDS program identifier.
  // Default: 0x40A7
  TX_RDS_PI = 0x2C01,
  // Si4713 Only. Configures mix of RDS PS Group with RDS Group Buffer.
  // Default: 0x0003
  TX_RDS_PS_MIX = 0x2C02,
  // Si4713 Only. Miscellaneous bits to transmit along with RDS_PS Groups.
  // Default: 0x1008
  TX_RDS_PS_MISC = 0x2C03,
  // Si4713 Only. Number of times to repeat transmission of a PS message before transmitting the next PS message.
  // Default: 0x0003
  TX_RDS_PS_REPEAT_COUNT = 0x2C04,
  // Si4713 Only. Number of PS messages in use.
  // Default: 0x0001
  TX_RDS_PS_MESSAGE_COUNT = 0x2C05,
  // Si4713 Only. RDS Program Service Alternate Frequency. This provides the ability to inform the receiver of a
  // single alternate frequency using AF Method A coding and is transmitted along with the RDS_PS Groups.
  // Default: 0xE0E0
  TX_RDS_PS_AF = 0x2C06,
  // Si4713 Only. Number of blocks reserved for the FIFO. Note that the value written must be one larger than the
  // desired FIFO size.
  // Default: 0x0000
  TX_RDS_FIFO_SIZE = 0x2C07,
};

// COMMANDS

struct CmdBase {
  CmdType CMD;
};

struct ResBase {
  union {
    uint8_t STATUS;
    struct {
      uint8_t STCINT : 1;  // Tune complete has been triggered
      uint8_t ASQINT : 1;  // Signal quality measurement has been triggered
      uint8_t RDSINT : 1;  // RDS interrupt has been triggered
      uint8_t RSQINT : 1;
      uint8_t _D4 : 1;
      uint8_t _D5 : 1;
      uint8_t ERR : 1;  // Error
      uint8_t CTS : 1;  // Clear to Send
    };
  };

  ResBase() { this->STATUS = 0; }
};

struct CmdPowerUp : CmdBase {
  union {
    uint8_t ARG1;
    struct {
      uint8_t FUNC : 4;     // Function (2 transmit, 15 query library)
      uint8_t XOSCEN : 1;   // Crystal Oscillator Enable
      uint8_t PATCH : 1;    // Patch Enable
      uint8_t GPO2OEN : 1;  // GPO2 Output Enable
      uint8_t CTSIEN : 1;   // CTS Interrupt Enable
    };
  };
  union {
    uint8_t ARG2;
    struct {
      uint8_t OPMODE : 8;  // Application Setting (0x50 analog, 0x0F digital)
    };
  };

  CmdPowerUp() {
    this->CMD = CmdType::POWER_UP;
    this->ARG1 = 0;
    this->ARG1 = 0;
  }
};

struct ResPowerUpTX : ResBase {};

struct ResPowerUpQueryLibrary : ResBase {
  union {
    uint8_t RESP1;
    struct {
      uint8_t PN : 8;  // Final 2 digits of part number
    };
  };
  union {
    uint8_t RESP2;
    struct {
      uint8_t FWMAJOR : 8;  // Firmware Major Revision
    };
  };
  union {
    uint8_t RESP3;
    struct {
      uint8_t FWMINOR : 8;  // Firmware Minor Revision
    };
  };
  uint8_t RESP4;  // Reserved
  uint8_t RESP5;  // Reserved
  union {
    uint8_t RESP6;
    struct {
      uint8_t CHIPREV : 8;  // Chip Revision
    };
  };
  union {
    uint8_t RESP7;
    struct {
      uint8_t LIBRARYID : 8;  // Library Revision
    };
  };
};

struct CmdGetRev : CmdBase {
  CmdGetRev() { this->CMD = CmdType::GET_REV; }
};

struct ResGetRev : ResBase {
  union {
    uint8_t RESP1;
    struct {
      uint8_t PN : 8;  // Final 2 digits of part number
    };
  };
  union {
    uint8_t RESP2;
    struct {
      uint8_t FWMAJOR : 8;  // Firmware Major Revision
    };
  };
  union {
    uint8_t RESP3;
    struct {
      uint8_t FWMINOR : 8;  // Firmware Minor Revision
    };
  };
  union {
    uint8_t RESP4;
    struct {
      uint8_t PATCHH : 8;  // Patch ID High Byte
    };
  };
  union {
    uint8_t RESP5;
    struct {
      uint8_t PATCHL : 8;  // Patch ID Low Byte
    };
  };
  union {
    uint8_t RESP6;
    struct {
      uint8_t CMPMAJOR : 8;  // Component Major Revision
    };
  };
  union {
    uint8_t RESP7;
    struct {
      uint8_t CMPMINOR : 8;  // Component Minor Revision
    };
  };
  union {
    uint8_t RESP8;
    struct {
      uint8_t CHIPREV : 8;  // Chip Revision
    };
  };
};

struct CmdPowerDown : CmdBase {
  CmdPowerDown() { this->CMD = CmdType::POWER_DOWN; }
};

struct ResPowerDown : ResBase {};

struct CmdSetProperty : CmdBase {
  uint8_t ARG1;  // zero
  union {
    uint8_t ARG2;
    struct {
      uint8_t PROPH : 8;
    };
  };
  union {
    uint8_t ARG3;
    struct {
      uint8_t PROPL : 8;
    };
  };
  union {
    uint8_t ARG4;
    struct {
      uint8_t PROPDH : 8;
    };
  };
  union {
    uint8_t ARG5;
    struct {
      uint8_t PROPDL : 8;
    };
  };

  CmdSetProperty() {
    this->CMD = CmdType::SET_PROPERTY;
    this->ARG1 = 0;
    this->ARG2 = 0;
    this->ARG3 = 0;
    this->ARG4 = 0;
    this->ARG5 = 0;
  }

  CmdSetProperty(PropType prop, uint16_t value) {
    this->CMD = CmdType::SET_PROPERTY;
    this->ARG1 = 0;
    this->ARG2 = ((uint16_t) prop) >> 8;
    this->ARG3 = ((uint16_t) prop) & 0xff;
    this->ARG4 = value >> 8;
    this->ARG5 = value & 0xff;
  }
};

struct ResSetProperty : ResBase {};

struct CmdGetProperty : CmdBase {
  uint8_t ARG1;  // zero
  union {
    uint8_t ARG2;
    struct {
      uint8_t PROPH : 8;
    };
  };
  union {
    uint8_t ARG3;
    struct {
      uint8_t PROPL : 8;
    };
  };

  CmdGetProperty() {
    this->CMD = CmdType::GET_PROPERTY;
    this->ARG1 = 0;
    this->ARG2 = 0;
    this->ARG3 = 0;
  }

  CmdGetProperty(PropType prop) {
    this->CMD = CmdType::GET_PROPERTY;
    this->ARG1 = 0;
    this->ARG2 = (uint16_t) prop >> 8;
    this->ARG3 = (uint16_t) prop & 0xff;
  }
};

struct ResGetProperty : ResBase {
  uint8_t RESP1;  // zero
  union {
    uint8_t RESP2;
    struct {
      uint8_t PROPDH : 8;  // Property Get High Byte
    };
  };
  union {
    uint8_t RESP3;
    struct {
      uint8_t PROPDL : 8;  // Property Get Low Byte
    };
  };

  operator uint16_t() const { return ((uint16_t) PROPDH << 8) | PROPDL; }
};

struct CmdGetIntStatus : CmdBase {
  CmdGetIntStatus() { this->CMD = CmdType::GET_INT_STATUS; }
};

struct ResGetIntStatus : ResBase {};

struct CmdTxTuneFreq : CmdBase {
  uint8_t ARG1;  // zero
  union {
    uint8_t ARG2;
    struct {
      uint8_t FREQH : 8;  // Tune Frequency High Byte
    };
  };
  union {
    uint8_t ARG3;
    struct {
      uint8_t FREQL : 8;  // Tune Frequency Low Byte
    };
  };

  CmdTxTuneFreq(uint16_t freq = 0) {  // 7600 to 10800, 10 kHz resolution, multiples of 50 kHz
    this->CMD = CmdType::TX_TUNE_FREQ;
    this->ARG1 = 0;
    this->ARG2 = (uint16_t) freq >> 8;
    this->ARG3 = (uint16_t) freq & 0xff;
  }
};

struct ResTxTuneFreq : ResBase {};

struct CmdTxTunePower : CmdBase {
  uint8_t ARG1;  // zero
  uint8_t ARG2;  // zero
  union {
    uint8_t ARG3;
    struct {
      uint8_t RFdBuV : 8;  // Tune Power Byte, 88 to 115 dBuV
    };
  };
  union {
    uint8_t ARG4;
    struct {
      uint8_t ANTCAP : 8;  // Antenna Tuning Capacitor, 0.25 pF x 0 to 191
    };
  };

  CmdTxTunePower(uint8_t power = 0, uint8_t antcap = 0) {
    this->CMD = CmdType::TX_TUNE_POWER;
    this->ARG1 = 0;
    this->ARG2 = 0;
    this->ARG3 = power;
    this->ARG4 = antcap;
  }
};

struct ResTxTunePower : ResBase {};

struct CmdTxTuneMeasure : CmdBase {
  uint8_t ARG1;  // zero
  union {
    uint8_t ARG2;
    struct {
      uint8_t FREQH : 8;  // Tune Frequency High Byte
    };
  };
  union {
    uint8_t ARG3;
    struct {
      uint8_t FREQL : 8;  // Tune Frequency Low Byte
    };
  };
  union {
    uint8_t ARG4;
    struct {
      uint8_t ANTCAP : 8;  // Antenna Tuning Capacitor, 0.25 pF x 0 to 191
    };
  };

  CmdTxTuneMeasure(uint8_t freq = 0, uint8_t antcap = 0) {
    this->CMD = CmdType::TX_TUNE_MEASURE;
    this->ARG1 = 0;
    this->ARG2 = (uint16_t) freq >> 8;
    this->ARG3 = (uint16_t) freq & 0xff;
    this->ARG4 = antcap;
  }
};

struct ResTxTuneMeasure : ResBase {};

struct CmdTxTuneStatus : CmdBase {
  uint8_t ARG1;  // zero
  union {
    uint8_t ARG2;
    struct {
      uint8_t INTACK : 1;  // Clears the seek/tune complete interrupt status indicator
      uint8_t RESERVED : 7;
    };
  };

  CmdTxTuneStatus() {
    this->CMD = CmdType::TX_TUNE_STATUS;
    this->ARG1 = 0;
    this->ARG2 = 0;
  }
};

struct ResTxTuneStatus : ResBase {
  uint8_t RESP1;
  union {
    uint8_t RESP2;
    struct {
      uint8_t READFREQH : 8;  // Read Frequency High Byte
    };
  };
  union {
    uint8_t RESP3;
    struct {
      uint8_t READFREQL : 8;  // Read Frequency Low Byte
    };
  };
  uint8_t RESP4;
  union {
    uint8_t RESP5;
    struct {
      uint8_t RFdBuV : 8;  // Read Power
    };
  };
  union {
    uint8_t RESP6;
    struct {
      uint8_t ANTCAP : 8;  // Read Antenna Tuning Capacitor
    };
  };
  union {
    uint8_t RESP7;
    struct {
      uint8_t RNL : 8;  // Read Received Noise Level (Si4712/13 Only)
    };
  };
};

struct CmdTxAsqStatus : CmdBase {
  uint8_t ARG1;  // zero
  union {
    uint8_t ARG2;
    struct {
      uint8_t INTACK : 1;  // Clears the seek/tune complete interrupt status indicator
      uint8_t RESERVED : 7;
    };
  };

  CmdTxAsqStatus() {
    this->CMD = CmdType::TX_ASQ_STATUS;
    this->ARG1 = 0;
    this->ARG2 = 0;
  }
};

struct ResTxAsqStatus : ResBase {
  union {
    uint8_t RESP1;
    struct {
      uint8_t OVERMOD : 1;  // Overmodulation Detection
      uint8_t IALH : 1;     // Input Audio Level Threshold Detect High
      uint8_t IALL : 1;     // Input Audio Level Threshold Detect Low
      uint8_t RESERVED : 5;
    };
  };
  uint8_t RESP2;
  uint8_t RESP3;
  union {
    uint8_t RESP4;
    struct {
      int8_t INLEVEL : 8;  // Input Audio Level
    };
  };
};

struct CmdTxRdsBuff : CmdBase {
  union {
    uint8_t ARG1;
    struct {
      uint8_t INTACK : 1;  // Clear RDS Group buffer interrupt
      uint8_t MTBUFF : 1;  // Empty RDS Group Buffer
      uint8_t LDBUFF : 1;  // Load RDS Group Buffer
      uint8_t RESERVED : 4;
      uint8_t FIFO : 1;  // Operate on FIFO
    };
  };
  union {
    uint8_t ARG2;
    struct {
      uint8_t RDSBH;  // RDS Block B High Byte
    };
  };
  union {
    uint8_t ARG3;
    struct {
      uint8_t RDSBL;  // RDS Block B Low Byte
    };
  };
  union {
    uint8_t ARG4;
    struct {
      uint8_t RDSCH;  // RDS Block C High Byte
    };
  };
  union {
    uint8_t ARG5;
    struct {
      uint8_t RDSCL;  // RDS Block C Low Byte
    };
  };
  union {
    uint8_t ARG6;
    struct {
      uint8_t RDSDH;  // RDS Block D High Byte
    };
  };
  union {
    uint8_t ARG7;
    struct {
      uint8_t RDSDL;  // RDS Block D Low Byte
    };
  };

  CmdTxRdsBuff() {
    this->CMD = CmdType::TX_RDS_BUFF;
    this->ARG1 = 0;
    this->ARG2 = 0;
    this->ARG3 = 0;
    this->ARG4 = 0;
    this->ARG5 = 0;
    this->ARG6 = 0;
    this->ARG7 = 0;
  }
};

struct ResTxRdsBuff : ResBase {
  union {
    uint8_t RESP1;
    struct {
      uint8_t FIFOMT : 1;     //  Interrupt source: RDS Group FIFO Buffer is empty
      uint8_t CBUFWRAP : 1;   //  Interrupt source: RDS Group Circular Buffer has wrapped
      uint8_t FIFOXMIT : 1;   //  Interrupt source: RDS Group has been transmitted from the circular buffer
      uint8_t CBUFXMIT : 1;   // Interrupt source: RDS Group has been transmitted from the FIFO buffer
      uint8_t RDSPSXMIT : 1;  //  Interrupt source: RDS PS Group has been transmitted
      uint8_t RESERVED : 3;
    };
  };
  union {
    uint8_t RESP2;
    struct {
      uint8_t CBAVAIL : 8;  // Returns the number of available Circular Buffer blocks
    };
  };
  union {
    uint8_t RESP3;
    struct {
      uint8_t CBUSED : 8;  //  Returns the number of used Circular Buffer blocks
    };
  };
  union {
    uint8_t RESP4;
    struct {
      uint8_t FIFOAVAIL : 8;  // Returns the number of available FIFO blocks
    };
  };
  union {
    uint8_t RESP5;
    struct {
      uint8_t FIFOUSED : 8;  //  Returns the number of used FIFO blocks
    };
  };
};

struct CmdTxRdsPs : CmdBase {
  union {
    uint8_t ARG1;
    struct {
      // Selects which PS data to load (0-23)
      // 0 = First 4 characters of PS0
      // 1 = Last 4 characters of PS0
      // 2 = First 4 characters of PS1
      // 3 = Last 4 characters of PS1
      // ...
      // 22 = First 4 characters of PS11
      // 23 = Last 4 characters of PS11
      uint8_t PSID : 4;
      uint8_t RESERVED : 4;
    };
  };
  union {
    uint8_t ARG2;
    struct {
      uint8_t PSCHAR0;  // RDS PSID CHAR0
    };
  };
  union {
    uint8_t ARG3;
    struct {
      uint8_t PSCHAR1;  // RDS PSID CHAR1
    };
  };
  union {
    uint8_t ARG4;
    struct {
      uint8_t PSCHAR2;  // RDS PSID CHAR2
    };
  };
  union {
    uint8_t ARG5;
    struct {
      uint8_t PSCHAR3;  // RDS PSID CHAR3
    };
  };

  CmdTxRdsPs() {
    this->CMD = CmdType::TX_RDS_PS;
    this->ARG1 = 0;
    this->ARG2 = 0;
    this->ARG3 = 0;
    this->ARG4 = 0;
    this->ARG5 = 0;
  }
};

struct ResTxRdsPs : ResBase {};

struct CmdGpioCtl : CmdBase {
  union {
    uint8_t ARG1;
    struct {
      uint8_t RESERVED1 : 1;
      uint8_t GPO1OEN : 1;  // GPO1 Output Enable
      uint8_t GPO2OEN : 1;  // GPO2 Output Enable
      uint8_t GPO3OEN : 1;  // GPO3 Output Enable
    };
  };

  CmdGpioCtl() {
    this->CMD = CmdType::GPIO_CTL;
    this->ARG1 = 0;
  }
};

struct ResGpioCtl : ResBase {};

struct CmdGpioSet : CmdBase {
  union {
    uint8_t ARG1;
    struct {
      uint8_t RESERVED1 : 1;
      uint8_t GPO1OEN : 1;  // GPO1 Output Level
      uint8_t GPO2OEN : 1;  // GPO2 Output Level
      uint8_t GPO3OEN : 1;  // GPO3 Output Level
      uint8_t RESERVED2 : 4;
    };
  };

  CmdGpioSet() {
    this->CMD = CmdType::GPIO_SET;
    this->ARG1 = 0;
  }
};

struct ResGpioSet : ResBase {};

// PROPERTIES

// TODO: init to datasheet defaults in the constructors

struct PropBase {
  PropType PROP;
};

struct PropGpoIen : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t STCIEN : 1;     // Seek/Tune Complete Interrupt Enable
      uint16_t ASQIEN : 1;     // Audio Signal Quality Interrupt Enable
      uint16_t RDSIEN : 1;     // RDS Interrupt Enable (Si4711/13/21 Only)
      uint16_t RESERVED1 : 3;  //
      uint16_t ERRIEN : 1;     // ERR Interrupt Enable
      uint16_t CTSIEN : 1;     // CTS Interrupt Enable
      uint16_t STCREP : 1;     // STC Interrupt Repeat
      uint16_t ASQREP : 1;     // ASQ Interrupt Repeat
      uint16_t RDSREP : 1;     // RDS Interrupt Repeat. (Si4711/13/21 Only)
      uint16_t RESERVED2 : 5;
    };
  };

  PropGpoIen() {
    this->PROP = PropType::GPO_IEN;
    this->PROPD = 0x0000;
  }
};

struct PropDigitalInputFormat : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t ISIZE : 2;  // Digital Audio Sample Precision
      uint16_t IMONO : 1;  // Mono Audio Mode
      uint16_t IMODE : 4;  // Digital Mode
      uint16_t IFALL : 1;  // DCLK Falling Edge
    };
  };

  PropDigitalInputFormat() {
    this->PROP = PropType::DIGITAL_INPUT_FORMAT;
    this->PROPD = 0x0000;
  }
};

// TX_TUNE_FREQ command must be sent to start the internal clocking before setting this
struct PropDigitalInputSampleRate : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t DISR : 16;  // Digital Input Sample Rate (32000 to 48000 Hz, 0 Disabled)
    };
  };

  PropDigitalInputSampleRate(uint16_t disr = 0) {
    this->PROP = PropType::DIGITAL_INPUT_SAMPLE_RATE;
    // this->PROPD = 0x0000;
    this->DISR = disr;
  }
};

struct PropRefClkFreq : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t REFCLKF : 16;  // Frequency of Reference Clock (31130 to 34406 Hz, 0 Disabled)
    };
  };

  PropRefClkFreq(uint16_t refclkf = 32768) {
    this->PROP = PropType::REFCLK_FREQ;
    // this->PROPD = 0x8000;
    this->REFCLKF = refclkf;
  }
};

struct PropRefClkPreScale : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RCLKP : 12;   // Prescaler for Reference Clock (31130 and 34406 Hz, 0 Disabled)
      uint16_t RCLKSEL : 1;  // RCLK/DCLK pin is clock source
      uint16_t RESERVED : 3;
    };
  };

  PropRefClkPreScale() {
    this->PROP = PropType::REFCLK_PRESCALE;
    this->PROPD = 0x0001;
  }
};

struct PropTxComponentEnable : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t PILOT : 1;  // Pilot Tone
      uint16_t LMR : 1;    // Left Minus Right
      uint16_t RDS : 1;    // RDS Enable
      uint16_t RESERVED : 13;
    };
  };

  PropTxComponentEnable() {
    this->PROP = PropType::TX_COMPONENT_ENABLE;
    this->PROPD = 0x0003;
  }
};

struct PropTxAudioDeviation : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t TXADEV : 16;  // Transmit Audio Frequency Deviation (0 Hz to 90 kHz in 10 Hz units)
    };
  };

  PropTxAudioDeviation(uint16_t txadev = 6825) {
    this->PROP = PropType::TX_AUDIO_DEVIATION;
    this->PROPD = 0x1AA9;
    this->TXADEV = txadev;
  }
};

struct PropTxPilotDeviation : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t TXPDEV : 16;  // Transmit Pilot Frequency Deviation (0 Hz to 90 kHz in 10 Hz units)
    };
  };

  PropTxPilotDeviation(uint16_t txpdev = 675) {
    this->PROP = PropType::TX_PILOT_DEVIATION;
    this->PROPD = 0x02A3;
    this->TXPDEV = txpdev;
  }
};

struct PropTxRdsDeviation : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t TXRDEV : 16;  // Transmit RDS Frequency Deviation (0 Hz to 90 kHz in 10 Hz units)
    };
  };

  PropTxRdsDeviation(uint16_t txrdev = 200) {
    this->PROP = PropType::TX_RDS_DEVIATION;
    this->PROPD = 0x00C8;
    this->TXRDEV = txrdev;
  }
};

struct PropTxLineInputLevel : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t LILEVEL : 10;  // Line Level (mV)
      uint16_t RESERVED1 : 2;
      uint16_t LIATTEN : 2;  // Line Attenuation (for 190, 301, 416, 636 mV)
      uint16_t RESERVED2 : 2;
    };
  };

  PropTxLineInputLevel() {
    this->PROP = PropType::TX_LINE_INPUT_LEVEL;
    this->PROPD = 0x327C;
  }
};

struct PropTxLineInputMute : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RIMUTE : 1;  // Mutes R Line Input
      uint16_t LIMUTE : 1;  // Mutes L Line Input
      uint16_t RESERVED : 14;
    };
  };

  PropTxLineInputMute() {
    this->PROP = PropType::TX_LINE_INPUT_MUTE;
    this->PROPD = 0x0000;
  }
};

struct PropTxPreEmphasis : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t FMPE : 2;  // FM Pre-Emphasis (75us, 50us, Disabled, Reserved)
      uint16_t RESERVED : 14;
    };
  };

  PropTxPreEmphasis() {
    this->PROP = PropType::TX_PREEMPHASIS;
    this->PROPD = 0x0000;
  }
};

struct PropTxPilotFrequency : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t FREQ : 16;  // Stereo Pilot Frequency (0 to 19000 Hz)
    };
  };

  PropTxPilotFrequency() {
    this->PROP = PropType::TX_PILOT_FREQUENCY;
    this->PROPD = 0x4A38;
  }
};

struct PropTxAcompEnable : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t ACEN : 1;     // Transmit Audio Dynamic Range Control Enable
      uint16_t LIMITEN : 1;  // Audio Limiter
      uint16_t RESERVED : 14;
    };
  };

  PropTxAcompEnable() {
    this->PROP = PropType::TX_ACOMP_ENABLE;
    this->PROPD = 0x0002;
  }
};

struct PropTxAcompThreshold : PropBase {
  union {
    uint16_t PROPD;
    struct {
      int16_t THRESHOLD : 16;  // Transmit Audio Dynamic Range Control Threshold (-40 to 0 dBFS)
    };
  };

  PropTxAcompThreshold() {
    this->PROP = PropType::TX_ACOMP_THRESHOLD;
    this->PROPD = 0xFFD8;
  }
};

struct PropTxAcompAttackTime : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t ATTACK : 4;  // Transmit Audio Dynamic Range Control Attack Time (0.5 (=0) to 5ms (=9))
      uint16_t RESERVED : 12;
    };
  };

  PropTxAcompAttackTime() {
    this->PROP = PropType::TX_ACOMP_ATTACK_TIME;
    this->PROPD = 0x0000;
  }
};

struct PropTxAcompReleaseTime : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RELEASE : 3;  // Transmit Audio Dynamic Range Control Release Time (100, 200, 350, 525, 1000 ms)
      uint16_t RESERVED : 13;
    };
  };

  PropTxAcompReleaseTime() {
    this->PROP = PropType::TX_ACOMP_RELEASE_TIME;
    this->PROPD = 0x0004;
  }
};

struct PropTxAcompGain : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t GAIN : 6;  // Transmit Audio Dynamic Range Control Gain (0 to 20 dB)
      uint16_t RESERVED : 10;
    };
  };

  PropTxAcompGain() {
    this->PROP = PropType::TX_ACOMP_GAIN;
    this->PROPD = 0x000F;
  }
};
struct PropTxLimiterReleaseTime : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t LMITERTC : 16;  // Sets the limiter release time
    };
  };

  PropTxLimiterReleaseTime() {
    this->PROP = PropType::TX_LIMITER_RELEASE_TIME;
    this->PROPD = 0x0066;
  }
};

struct PropTxAsqInterruptSource : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t IALLIEN : 1;     // Input Audio Level Detection Low Threshold Enable
      uint16_t IALHIEN : 1;     // Input Audio Level Detection High Threshold Enable
      uint16_t OVERMODIEN : 1;  // Overmodulation Detection Enable
      uint16_t RESERVED : 13;
    };
  };

  PropTxAsqInterruptSource() {
    this->PROP = PropType::TX_ASQ_INTERRUPT_SOURCE;
    this->PROPD = 0x0000;
  }
};

struct PropTxAsqLevelLow : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t IALLTH : 8;  // Input Audio Level Low Threshold (-70 to 0 dB)
      uint16_t RESERVED : 8;
    };
  };

  PropTxAsqLevelLow() {
    this->PROP = PropType::TX_ASQ_LEVEL_LOW;
    this->PROPD = 0x0000;
  }
};

struct PropTxAsqDurationLow : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t IALLDUR : 16;  // Input Audio Level Duration Low (0 to 65535 ms)
    };
  };

  PropTxAsqDurationLow() {
    this->PROP = PropType::TX_ASQ_DURATION_LOW;
    this->PROPD = 0x0000;
  }
};

struct PropTxAsqDurationHigh : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t IALHTH : 8;  // Input Audio Level High Threshold (-70 to 0 dB)
      uint16_t RESERVED : 8;
    };
  };

  PropTxAsqDurationHigh() {
    this->PROP = PropType::TX_ASQ_LEVEL_HIGH;
    this->PROPD = 0x0000;
  }
};

struct PropTxAsqDurationHigh : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t IALHDUR : 16;  // Input Audio Level Duration High (0 to 65535 ms)
    };
  };

  PropTxAsqDurationHigh() {
    this->PROP = PropType::TX_ASQ_DURATION_HIGH;
    this->PROPD = 0x0000;
  }
};

struct PropTxRdsInterruptSource : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RDSFIFOMT : 1;    //  Interrupt when the RDS Group FIFO Buffer is empty
      uint16_t RDSCBUFWRAP : 1;  // Interrupt when the RDS Group Circular Buffer has wrapped
      uint16_t RDSFIFOXMIT : 1;  // Interrupt when a RDS Group has been transmitted from the FIFO Buffer
      uint16_t RDSCBUFXMIT : 1;  // Interrupt when a RDS Group has been transmitted from the Circular Buffer
      uint16_t RDSPSXMIT : 1;    // Interrupt when a RDS PS Group has been transmitted
      uint16_t RESERVED : 11;
    };
  };

  PropTxRdsInterruptSource() {
    this->PROP = PropType::TX_RDS_INTERRUPT_SOURCE;
    this->PROPD = 0x0000;
  }
};

struct PropTxRdsPi : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RDSPI : 16;  // Transmit RDS Program Identifier
    };
  };

  PropTxRdsPi() {
    this->PROP = PropType::TX_RDS_PI;
    this->PROPD = 0x40A7;
  }
};

struct PropTxRdsPsMix : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RDSPSMIX : 3;  // Transmit RDS Mix
      uint16_t RESERVED : 13;
    };
  };

  PropTxRdsPsMix() {
    this->PROP = PropType::TX_RDS_PS_MIX;
    this->PROPD = 0x0003;
  }
};

struct PropTxRdsPsMisc : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RESERVED : 3;
      uint16_t RDSMS : 1;   // Music/Speech Switch Code
      uint16_t RDSTA : 1;   // Traffic Announcement Code
      uint16_t RDSPTY : 1;  // Program Type Code
      uint16_t RDSTP : 5;   // Traffic Program Code
      uint16_t FORCEB : 1;  // Use the PTY and TP set here in all block B data
      uint16_t RDSD0 : 1;   // Mono/Stereo code
      uint16_t RDSD1 : 1;   // Artificial Head code
      uint16_t RDSD2 : 1;   // Compressed code
      uint16_t RDSD3 : 1;   // Dynamic PTY code
    };
  };

  PropTxRdsPsMisc() {
    this->PROP = PropType::TX_RDS_PS_MISC;
    this->PROPD = 0x1008;
  }
};

struct PropTxRdsRepeatCount : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RDSPSRC : 8;  // Transmit RDS PS Repeat Count
      uint16_t RESERVED : 8;
    };
  };

  PropTxRdsRepeatCount() {
    this->PROP = PropType::TX_RDS_PS_REPEAT_COUNT;
    this->PROPD = 0x0003;
  }
};

struct PropTxRdsMessageCount : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RDSPSMC : 4;  // Transmit RDS PS Message Count.
      uint16_t RESERVED : 12;
    };
  };

  PropTxRdsMessageCount() {
    this->PROP = PropType::TX_RDS_PS_MESSAGE_COUNT;
    this->PROPD = 0x0001;
  }
};

struct PropTxRdsPsAf : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RDSAF : 16;  // Transmit RDS Program Service Alternate Frequency
    };
  };

  PropTxRdsPsAf() {
    this->PROP = PropType::TX_RDS_PS_AF;
    this->PROPD = 0xE0E0;
  }
};

struct PropTxRdsFifoSize : PropBase {
  union {
    uint16_t PROPD;
    struct {
      uint16_t RDSFIFOSZ : 8;  // Transmit RDS FIFO Size (0 = FIFO disabled)
      uint16_t RESERVED : 8;
    };
  };

  PropTxRdsFifoSize() {
    this->PROP = PropType::TX_RDS_FIFO_SIZE;
    this->PROPD = 0x0000;
  }
};

}  // namespace si4713
}  // namespace esphome

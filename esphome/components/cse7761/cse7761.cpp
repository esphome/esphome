#include "cse7761.h"

#include "esphome/core/log.h"

namespace esphome {
namespace cse7761 {

static const char *const TAG = "cse7761";

/*********************************************************************************************\
 * CSE7761 - Energy  (Sonoff Dual R3 Pow v1.x)
 *
 * Based on Tasmota source code
 * See https://github.com/arendst/Tasmota/discussions/10793
 * https://github.com/arendst/Tasmota/blob/development/tasmota/tasmota_xnrg_energy/xnrg_19_cse7761.ino
\*********************************************************************************************/

static const int CSE7761_UREF = 42563;    // RmsUc
static const int CSE7761_IREF = 52241;    // RmsIAC
static const int CSE7761_PREF = 44513;    // PowerPAC
static const int CSE7761_FREF = 3579545;  // System clock (3.579545MHz) as used in frequency calculation

static const uint8_t CSE7761_REG_SYSCON = 0x00;     // (2) System Control Register (0x0A04)
static const uint8_t CSE7761_REG_EMUCON = 0x01;     // (2) Metering control register (0x0000)
static const uint8_t CSE7761_REG_EMUCON2 = 0x13;    // (2) Metering control register 2 (0x0001)
static const uint8_t CSE7761_REG_PULSE1SEL = 0x1D;  // (2) Pin function output select register (0x3210)

static const uint8_t CSE7761_REG_UFREQ = 0x23;      // (2) Voltage Frequency (0x0000)
static const uint8_t CSE7761_REG_RMSIA = 0x24;      // (3) The effective value of channel A current (0x000000)
static const uint8_t CSE7761_REG_RMSIB = 0x25;      // (3) The effective value of channel B current (0x000000)
static const uint8_t CSE7761_REG_RMSU = 0x26;       // (3) Voltage RMS (0x000000)
static const uint8_t CSE7761_REG_POWERPA = 0x2C;    // (4) Channel A active power, update rate 27.2Hz (0x00000000)
static const uint8_t CSE7761_REG_POWERPB = 0x2D;    // (4) Channel B active power, update rate 27.2Hz (0x00000000)
static const uint8_t CSE7761_REG_SYSSTATUS = 0x43;  // (1) System status register

// static const uint8_t CSE7761_REG_COEFFOFFSET    = 0x6E;     // (2) Coefficient checksum offset (0xFFFF)
static const uint8_t CSE7761_REG_COEFFCHKSUM = 0x6F;  // (2) Coefficient checksum
static const uint8_t CSE7761_REG_RMSIAC = 0x70;       // (2) Channel A effective current conversion coefficient
// static const uint8_t CSE7761_REG_RMSIBC         = 0x71;     // (2) Channel B effective current conversion coefficient
// static const uint8_t CSE7761_REG_RMSUC          = 0x72;     // (2) Effective voltage conversion coefficient
// static const uint8_t CSE7761_REG_POWERPAC       = 0x73;     // (2) Channel A active power conversion coefficient
// static const uint8_t CSE7761_REG_POWERPBC       = 0x74;     // (2) Channel B active power conversion coefficient
// static const uint8_t CSE7761_REG_POWERSC        = 0x75;     // (2) Apparent power conversion coefficient
// static const uint8_t CSE7761_REG_ENERGYAC       = 0x76;     // (2) Channel A energy conversion coefficient
// static const uint8_t CSE7761_REG_ENERGYBC       = 0x77;     // (2) Channel B energy conversion coefficient

static const uint8_t CSE7761_SPECIAL_COMMAND = 0xEA;  // Start special command
static const uint8_t CSE7761_CMD_RESET = 0x96;        // Reset command, after receiving the command, the chip resets
// static const uint8_t CSE7761_CMD_CHAN_A_SELECT  = 0x5A;     // Current channel A setting command, which specifies the
// current used to calculate apparent power,
//                                                             //   Power factor, phase angle, instantaneous active
//                                                             power, instantaneous apparent power and
//                                                             //   The channel indicated by the signal of power
//                                                             overload is channel A
// static const uint8_t CSE7761_CMD_CHAN_B_SELECT  = 0xA5;     // Current channel B setting command, which specifies the
// current used to calculate apparent power,
//                                                             //   Power factor, phase angle, instantaneous active
//                                                             power, instantaneous apparent power and
//                                                             //   The channel indicated by the signal of power
//                                                             overload is channel B
static const uint8_t CSE7761_CMD_CLOSE_WRITE = 0xDC;   // Close write operation
static const uint8_t CSE7761_CMD_ENABLE_WRITE = 0xE5;  // Enable write operation

enum CSE7761 { RMS_IAC, RMS_IBC, RMS_UC, POWER_PAC, POWER_PBC, POWER_SC, ENERGY_AC, ENERGY_BC };

void CSE7761Component::setup() {
  this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_RESET);
  uint16_t syscon = this->read_(0x00, 2);  // Default 0x0A04
  if ((0x0A04 == syscon) && this->chip_init_()) {
    this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_CLOSE_WRITE);
    ESP_LOGD(TAG, "CSE7761 found");
    this->data_.ready = true;
  } else {
    this->mark_failed();
  }
}

void CSE7761Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CSE7761:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  this->check_uart_settings(38400, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
}

float CSE7761Component::get_setup_priority() const { return setup_priority::DATA; }

void CSE7761Component::update() {
  if (this->data_.ready) {
    this->get_data_();
  }
}

void CSE7761Component::write_(uint8_t reg, uint16_t data) {
  uint8_t buffer[5];

  buffer[0] = 0xA5;
  buffer[1] = reg;
  uint32_t len = 2;
  if (data) {
    if (data < 0xFF) {
      buffer[2] = data & 0xFF;
      len = 3;
    } else {
      buffer[2] = (data >> 8) & 0xFF;
      buffer[3] = data & 0xFF;
      len = 4;
    }
    uint8_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
      crc += buffer[i];
    }
    buffer[len] = ~crc;
    len++;
  }

  this->write_array(buffer, len);
}

bool CSE7761Component::read_once_(uint8_t reg, uint8_t size, uint32_t *value) {
  while (this->available()) {
    this->read();
  }

  this->write_(reg, 0);

  uint8_t buffer[8] = {0};
  uint32_t rcvd = 0;

  for (uint32_t i = 0; i <= size; i++) {
    int value = this->read();
    if (value > -1 && rcvd < sizeof(buffer) - 1) {
      buffer[rcvd++] = value;
    }
  }

  if (!rcvd) {
    ESP_LOGD(TAG, "Received 0 bytes for register %hhu", reg);
    return false;
  }

  rcvd--;
  uint32_t result = 0;
  // CRC check
  uint8_t crc = 0xA5 + reg;
  for (uint32_t i = 0; i < rcvd; i++) {
    result = (result << 8) | buffer[i];
    crc += buffer[i];
  }
  crc = ~crc;
  if (crc != buffer[rcvd]) {
    return false;
  }

  *value = result;
  return true;
}

uint32_t CSE7761Component::read_(uint8_t reg, uint8_t size) {
  bool result = false;  // Start loop
  uint8_t retry = 3;    // Retry up to three times
  uint32_t value = 0;   // Default no value
  while (!result && retry > 0) {
    retry--;
    if (this->read_once_(reg, size, &value))
      return value;
  }
  ESP_LOGE(TAG, "Reading register %hhu failed!", reg);
  return value;
}

uint32_t CSE7761Component::coefficient_by_unit_(uint32_t unit) {
  uint32_t coeff = 1;
  if (this->data_.model == CSE7761_MODEL_POWCT) {
    coeff = 5;
  }
  switch (unit) {
    case RMS_UC:
      return 0x400000 * 100 / this->data_.coefficient[RMS_UC];
    case RMS_IAC:
      return (0x800000 * 100 / (this->data_.coefficient[RMS_IAC] * coeff)) * 10;  // Stay within 32 bits
    case POWER_PAC:
      return 0x80000000 / (this->data_.coefficient[POWER_PAC] * coeff);
  }
  return 0;
}

bool CSE7761Component::chip_init_() {
  uint16_t calc_chksum = 0xFFFF;
  for (uint32_t i = 0; i < 8; i++) {
    this->data_.coefficient[i] = this->read_(CSE7761_REG_RMSIAC + i, 2);
    calc_chksum += this->data_.coefficient[i];
  }
  calc_chksum = ~calc_chksum;
  uint16_t coeff_chksum = this->read_(CSE7761_REG_COEFFCHKSUM, 2);
  if ((calc_chksum != coeff_chksum) || (!calc_chksum)) {
    ESP_LOGD(TAG, "Default calibration");
    this->data_.coefficient[RMS_IAC] = CSE7761_IREF;
    this->data_.coefficient[RMS_UC] = CSE7761_UREF;
    this->data_.coefficient[POWER_PAC] = CSE7761_PREF;
  }

  this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_ENABLE_WRITE);

  uint8_t sys_status = this->read_(CSE7761_REG_SYSSTATUS, 1);
  if (sys_status & 0x10) {  // Write enable to protected registers (WREN)

    /*
      System Control Register (SYSCON)  Addr:0x00  Default value: 0x0A04
      Bit    name               Function description
      15-11  NC                 -, the default is 1
      10     ADC2ON
                                =1, means ADC current channel B is on (Sonoff Dual R3)
                                =0, means ADC current channel B is closed (Pow CT)
      9      NC                 -, the default is 1.
      8-6    PGAIB[2:0]         Current channel B analog gain selection highest bit
                                =1XX, PGA of current channel B=16 (Sonoff Dual R3)
                                =011, PGA of current channel B=8
                                =010, PGA of current channel B=4
                                =001, PGA of current channel B=2
                                =000, PGA of current channel B=1 (Pow CT)
      5-3    PGAU[2:0]          Highest bit of voltage channel analog gain selection
                                =1XX, PGA of voltage U=16
                                =011, PGA of voltage U=8
                                =010, PGA of voltage U=4
                                =001, PGA of voltage U=2
                                =000, PGA of voltage U=1 (Sonoff Dual R3 / Pow CT)
      2-0    PGAIA[2:0]         Current channel A analog gain selection highest bit
                                =1XX, PGA of current channel A=16 (Sonoff Dual R3)
                                =011, PGA of current channel A=8
                                =010, PGA of current channel A=4
                                =001, PGA of current channel A=2
                                =000, PGA of current channel A=1 (Pow CT)
    */

    if (this->data_.model == CSE7761_MODEL_POWCT) {
      this->write_(CSE7761_REG_SYSCON | 0x80, 0xFE00);  // POW CT + enable channel B
    } else {
      this->write_(CSE7761_REG_SYSCON | 0x80, 0xFF04);  // Sonoff Dual R3
    }

    /*
      Energy Measure Control Register (EMUCON)  Addr:0x01  Default value: 0x0000
      Bit    name               Function description
      15-14  Tsensor_Step[1:0]  Measurement steps of temperature sensor:
                                =2'b00 The first step of temperature sensor measurement, the Offset of OP1 and OP2 is
      +/+. (Sonoff Dual R3 / Pow CT) =2'b01 The second step of temperature sensor measurement, the Offset of OP1 and OP2
      is +/-. =2'b10 The third step of temperature sensor measurement, the Offset of OP1 and OP2 is -/+. =2'b11 The
      fourth step of temperature sensor measurement, the Offset of OP1 and OP2 is -/-. After measuring these four
      results and averaging, the AD value of the current measured temperature can be obtained. 13     tensor_en
      Temperature measurement module control =0 when the temperature measurement module is closed; (Sonoff Dual R3 / Pow
      CT) =1 when the temperature measurement module is turned on; 12     comp_off           Comparator module close
      signal: =0 when the comparator module is in working state =1 when the comparator module is off (Sonoff Dual R3 /
      Pow CT) 11-10  Pmode[1:0]         Selection of active energy calculation method: Pmode =00, both positive and
      negative active energy participate in the accumulation, the accumulation method is algebraic sum mode, the reverse
      REVQ symbol indicates to active power; (Sonoff Dual R3 / Pow CT) Pmode = 01, only accumulate positive active
      energy; Pmode = 10, both positive and negative active energy participate in the accumulation, and the accumulation
      method is absolute value method. No reverse active power indication; Pmode =11, reserved, the mode is the same as
      Pmode =00 9      NC                 - 8      ZXD1               The initial value of ZX output is 0, and different
      waveforms are output according to the configuration of ZXD1 and ZXD0: =0, it means that the ZX output changes only
      at the selected zero-crossing point (Sonoff Dual R3 / Pow CT) =1, indicating that the ZX output changes at both
      the positive and negative zero crossings 7      ZXD0 =0, indicates that the positive zero-crossing point is
      selected as the zero-crossing detection signal (Sonoff Dual R3 / Pow CT) =1, indicating that the negative
      zero-crossing point is selected as the zero-crossing detection signal 6      HPFIBOFF =0, enable current channel B
      digital high-pass filter (Sonoff Dual R3) =1, turn off the digital high-pass filter of current channel B (Pow CT)
      5      HPFIAOFF
                                =0, enable current channel A digital high-pass filter (Sonoff Dual R3 / Pow CT)
                                =1, turn off the digital high-pass filter of current channel A
      4      HPFUOFF
                                =0, enable U channel digital high pass filter (Sonoff Dual R3 / Pow CT)
                                =1, turn off the U channel digital high-pass filter
      3-2    NC                 -
      1      PBRUN
                                =1, enable PFB pulse output and active energy register accumulation; (Sonoff Dual R3 /
      Pow CT) =0 (default), turn off PFB pulse output and active energy register accumulation. 0      PARUN =1, enable
      PFA pulse output and active energy register accumulation; (Sonoff Dual R3 / Pow CT) =0 (default), turn off PFA
      pulse output and active energy register accumulation.
    */

    this->write_(
        CSE7761_REG_EMUCON | 0x80,
        0x1183);  // Same as Sonoff Dual R3 (enable channel B) + zero crossing on both negative and positive signal

    /*
      Energy Measure Control Register (EMUCON2)  Addr: 0x13  Default value: 0x0001
      Bit    name               Function description
      15-13  NC                 -
      12     SDOCmos
                                =1, SDO pin CMOS open-drain output
                                =0, SDO pin CMOS output (Sonoff Dual R3 / Pow CT)
      11     EPB_CB             Energy_PB clear signal control, the default is 0, and it needs to be configured to 1 in
      UART mode. Clear after reading is not supported in UART mode =1, Energy_PB will not be cleared after reading;
      (Sonoff Dual R3 / Pow CT) =0, Energy_PB is cleared after reading; 10     EPA_CB             Energy_PA clear signal
      control, the default is 0, it needs to be configured to 1 in UART mode, Clear after reading is not supported in
      UART mode =1, Energy_PA will not be cleared after reading; (Sonoff Dual R3 / Pow CT) =0, Energy_PA is cleared
      after reading; 9-8    DUPSEL[1:0]        Average register update frequency control =00, Update frequency 3.4Hz
                                =01, Update frequency 6.8Hz
                                =10, Update frequency 13.65Hz
                                =11, Update frequency 27.3Hz (Sonoff Dual R3 / Pow CT)
      7      CHS_IB             Current channel B measurement selection signal
                                =1, measure the current of channel B (Sonoff Dual R3 / Pow CT)
                                =0, measure the internal temperature of the chip
      6      PfactorEN          Power factor function enable
                                =1, turn on the power factor output function (Sonoff Dual R3 / Pow CT)
                                =0, turn off the power factor output function
      5      WaveEN             Waveform data, instantaneous data output enable signal
                                =1, turn on the waveform data output function (if frequency enable)
                                =0, turn off the waveform data output function (Sonoff Dual R3 / Pow CT)
      4      SAGEN              Voltage drop detection enable signal, WaveEN=1 must be configured first
                                =1, turn on the voltage drop detection function
                                =0, turn off the voltage drop detection function (Sonoff Dual R3 / Pow CT)
      3      OverEN             Overvoltage, overcurrent, and overload detection enable signal, WaveEN=1 must be
      configured first =1, turn on the overvoltage, overcurrent, and overload detection functions =0, turn off the
      overvoltage, overcurrent, and overload detection functions (Sonoff Dual R3 / Pow CT) 2      ZxEN Zero-crossing
      detection, phase angle, voltage frequency measurement enable signal =1, turn on the zero-crossing detection, phase
      angle, and voltage frequency measurement functions (if frequency enable) =0, disable zero-crossing detection,
      phase angle, voltage frequency measurement functions (Sonoff Dual R3 / Pow CT) 1      PeakEN             Peak
      detect enable signal =1, turn on the peak detection function =0, turn off the peak detection function (Sonoff Dual
      R3 / Pow CT) 0      NC                 Default is 1
    */

    this->write_(CSE7761_REG_EMUCON2 | 0x80, 0x0FE5);  // Sonoff Dual R3 / Pow CT + frequency measure enable

    /*
      Pin function output selection register (PULSE1SEL)  Addr: 0x1D  Default value: 0x3210
      Bit    name               Function description
      15-13  NC                 -
      12     SDOCmos
                                =1, SDO pin CMOS open-drain output

      15-12  NC                 NC, the default value is 4'b0011
      11-8   NC                 NC, the default value is 4'b0010
      7-4    P2Sel              Pulse2 Pin output function selection, see the table below
      3-0    P1Sel              Pulse1 Pin output function selection, see the table below

      Table Pulsex function output selection list
      Pxsel  Select description
      0000   Output of energy metering calibration pulse PFA
      0001   The output of the energy metering calibration pulse PFB
      0010   Comparator indication signal comp_sign
      0011   Interrupt signal IRQ output (the default is high level, if it is an interrupt, set to 0)
      0100   Signal indication of power overload: only PA or PB can be selected
      0101   Channel A negative power indicator signal
      0110   Channel B negative power indicator signal
      0111   Instantaneous value update interrupt output
      1000   Average update interrupt output
      1001   Voltage channel zero-crossing signal output (Tasmota add zero-cross detection)
      1010   Current channel A zero-crossing signal output
      1011   Current channel B zero crossing signal output
      1100   Voltage channel overvoltage indication signal output
      1101   Voltage channel undervoltage indication signal output
      1110   Current channel A overcurrent signal indication output
      1111   Current channel B overcurrent signal indication output
    */

    // this->write_(CSE7761_REG_PULSE1SEL | 0x80, 0x3290);  // Enable zero crosing signal output on function pin

  } else {
    ESP_LOGD(TAG, "Write failed at chip_init");
    return false;
  }
  return true;
}

void CSE7761Component::get_data_() {
  // The effective value of current and voltage Rms is a 24-bit signed number,
  // the highest bit is 0 for valid data,
  //   and when the highest bit is 1, the reading will be processed as zero
  // The active power parameter PowerA/B is in two’s complement format, 32-bit
  // data, the highest bit is Sign bit.
  uint32_t value = this->read_(CSE7761_REG_RMSU, 3);
  this->data_.voltage_rms = (value >= 0x800000) ? 0 : value;

  value = this->read_(CSE7761_REG_UFREQ, 2);
  this->data_.frequency = (value >= 0x8000) ? 0 : value;

  value = this->read_(CSE7761_REG_RMSIA, 3);
  this->data_.current_rms[0] = ((value >= 0x800000) || (value < 1600)) ? 0 : value;  // No load threshold of 10mA
  value = this->read_(CSE7761_REG_POWERPA, 4);
  this->data_.active_power[0] = (0 == this->data_.current_rms[0]) ? 0 : ((uint32_t) abs((int) value));

  value = this->read_(CSE7761_REG_RMSIB, 3);
  this->data_.current_rms[1] = ((value >= 0x800000) || (value < 1600)) ? 0 : value;  // No load threshold of 10mA
  value = this->read_(CSE7761_REG_POWERPB, 4);
  this->data_.active_power[1] = (0 == this->data_.current_rms[1]) ? 0 : ((uint32_t) abs((int) value));

  // convert values and publish to sensors

  float voltage = (float) this->data_.voltage_rms / this->coefficient_by_unit_(RMS_UC);
  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(voltage);
  }

  float freq = (this->data_.frequency) ? ((float) CSE7761_FREF / 8 / this->data_.frequency) : 0;  // Hz
  if (this->frequency_sensor_ != nullptr) {
    this->frequency_sensor_->publish_state(freq);
  }
  for (uint8_t channel = 0; channel < 2; channel++) {
    // Active power = PowerPA * PowerPAC * 1000 / 0x80000000
    float active_power = (float) this->data_.active_power[channel] / this->coefficient_by_unit_(POWER_PAC);  // W
    float amps = (float) this->data_.current_rms[channel] / this->coefficient_by_unit_(RMS_IAC);             // A
    ESP_LOGD(TAG, "Channel %d power %f W, current %f A", channel + 1, active_power, amps);
    if (channel == 0) {
      if (this->power_sensor_1_ != nullptr) {
        this->power_sensor_1_->publish_state(active_power);
      }
      if (this->current_sensor_1_ != nullptr) {
        this->current_sensor_1_->publish_state(amps);
      }
    } else if (channel == 1) {
      if (this->power_sensor_2_ != nullptr) {
        this->power_sensor_2_->publish_state(active_power);
      }
      if (this->current_sensor_2_ != nullptr) {
        this->current_sensor_2_->publish_state(amps);
      }
    }
  }
}

}  // namespace cse7761
}  // namespace esphome

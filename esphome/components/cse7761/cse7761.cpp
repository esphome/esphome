#include "cse7761.h"

#include "esphome/core/log.h"

namespace esphome::cse7761 {

static const char *const TAG = "cse7761";

/*********************************************************************************************\
 * CSE7761 - Energy  (Sonoff Dual R3 Pow v1.x)
 *
 * Based on Tasmota source code
 * See https://github.com/arendst/Tasmota/discussions/10793
 * https://github.com/arendst/Tasmota/blob/development/tasmota/xnrg_19_cse7761.ino
\*********************************************************************************************/

static constexpr int CSE7761_UREF = 42563;  // RmsUc
static constexpr int CSE7761_IREF = 52241;  // RmsIAC
static constexpr int CSE7761_PREF = 44513;  // PowerPAC

// System clock (3.579545MHz), used to convert the raw Ufreq register into a line frequency in Hz.
static constexpr uint32_t CSE7761_SYSTEM_CLOCK = 3579545;

static constexpr uint8_t CSE7761_REG_SYSCON = 0x00;     // (2) System Control Register (0x0A04)
static constexpr uint8_t CSE7761_REG_EMUCON = 0x01;     // (2) Metering control register (0x0000)
static constexpr uint8_t CSE7761_REG_HFCONST = 0x02;    // (2) Pulse frequency register (0x1000)
static constexpr uint8_t CSE7761_REG_EMUCON2 = 0x13;    // (2) Metering control register 2 (0x0001)
static constexpr uint8_t CSE7761_REG_PULSE1SEL = 0x1D;  // (2) Pin function output select register (0x3210)

static constexpr uint8_t CSE7761_REG_UFREQ = 0x23;        // (2) Voltage frequency (0x0000)
static constexpr uint8_t CSE7761_REG_RMSIA = 0x24;        // (3) The effective value of channel A current (0x000000)
static constexpr uint8_t CSE7761_REG_RMSIB = 0x25;        // (3) The effective value of channel B current (0x000000)
static constexpr uint8_t CSE7761_REG_RMSU = 0x26;         // (3) Voltage RMS (0x000000)
static constexpr uint8_t CSE7761_REG_POWERFACTOR = 0x27;  // (3) Power factor of the selected channel (0x7FFFFF)
static constexpr uint8_t CSE7761_REG_ENERGY_PA = 0x28;    // (3) Channel A active energy pulse count, cleared on
                                                          // overflow only (EPA_CB=1) (0x000000)
static constexpr uint8_t CSE7761_REG_ENERGY_PB = 0x29;    // (3) Channel B active energy pulse count, cleared on
                                                          // overflow only (EPB_CB=1) (0x000000)
static constexpr uint8_t CSE7761_REG_POWERPA = 0x2C;      // (4) Channel A active power, update rate 27.2Hz (0x00000000)
static constexpr uint8_t CSE7761_REG_POWERPB = 0x2D;      // (4) Channel B active power, update rate 27.2Hz (0x00000000)
static constexpr uint8_t CSE7761_REG_SYSSTATUS = 0x43;    // (1) System status register

static constexpr uint8_t CSE7761_REG_COEFFCHKSUM = 0x6F;  // (2) Coefficient checksum
static constexpr uint8_t CSE7761_REG_RMSIAC = 0x70;       // (2) Channel A effective current conversion coefficient

// Datasheet active energy formula denominator: K1*K2*2^29*4096 with K1=K2=1 (the same assumption
// coefficient_by_unit_() already makes for RMS/power), pre-scaled by 1e3 -- the formula's own
// trailing "*1000" already yields Wh directly (empirically confirmed against a real device; an
// earlier version of this code assumed it yielded kWh and scaled by another 1e3 too many).
static constexpr double CSE7761_ENERGY_WH_DIVISOR = 2199023255552.0 /* 2^41 */ / 1.0e3;

static constexpr uint8_t CSE7761_SPECIAL_COMMAND = 0xEA;  // Start special command
static constexpr uint8_t CSE7761_CMD_RESET = 0x96;        // Reset command, after receiving the command, the chip resets
static constexpr uint8_t CSE7761_CMD_CLOSE_WRITE = 0xDC;  // Close write operation
static constexpr uint8_t CSE7761_CMD_ENABLE_WRITE = 0xE5;  // Enable write operation

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
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Power Factor", this->power_factor_sensor_);
  LOG_SENSOR("  ", "Energy 1", this->energy_sensor_1_);
  LOG_SENSOR("  ", "Energy 2", this->energy_sensor_2_);
  this->check_uart_settings(38400, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
}

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
  uint32_t coeff = 0;
  switch (unit) {
    case RMS_UC:
      coeff = this->data_.coefficient[RMS_UC];
      return coeff ? 0x400000 * 100 / coeff : 0;
    case RMS_IAC:
      coeff = this->data_.coefficient[RMS_IAC];
      return coeff ? (0x800000 * 100 / coeff) * 10 : 0;  // Stay within 32 bits
    case POWER_PAC:
      coeff = this->data_.coefficient[POWER_PAC];
      return coeff ? 0x80000000 / coeff : 0;
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

  this->hf_const_ = this->read_(CSE7761_REG_HFCONST, 2);

  this->write_(CSE7761_SPECIAL_COMMAND, CSE7761_CMD_ENABLE_WRITE);

  uint8_t sys_status = this->read_(CSE7761_REG_SYSSTATUS, 1);
  if (sys_status & 0x10) {  // Write enable to protected registers (WREN)
    this->write_(CSE7761_REG_SYSCON | 0x80, 0xFF04);
    this->write_(CSE7761_REG_EMUCON | 0x80, 0x1183);
    // WaveEN=1, ZxEN=1: voltage frequency measurement requires the instantaneous data function
    // (WaveEN) to be enabled first, per the datasheet's zero-crossing/frequency section header.
    this->write_(CSE7761_REG_EMUCON2 | 0x80, 0x0FE5);
    this->write_(CSE7761_REG_PULSE1SEL | 0x80, 0x3290);
  } else {
    ESP_LOGD(TAG, "Write failed at chip_init");
    return false;
  }
  return true;
}

void CSE7761Component::accumulate_energy_(uint8_t channel, uint8_t reg) {
  // E_PA/E_PB are 24-bit pulse counters that free-run without clearing on read (EPA_CB/EPB_CB are
  // set in chip_init_(), as required for UART mode), so track deltas with 24-bit wraparound.
  uint32_t raw = this->read_(reg, 3);
  if (!this->energy_pulses_valid_[channel]) {
    this->last_energy_pulses_[channel] = raw;
    this->energy_pulses_valid_[channel] = true;
    return;
  }
  uint32_t delta = (raw - this->last_energy_pulses_[channel]) & 0xFFFFFF;
  this->last_energy_pulses_[channel] = raw;
  if (delta == 0)
    return;

  // Energy[Wh] = pulses * EnergyXC * HFConst / 2^41 * 1000, per the datasheet's active energy formula,
  // assuming K1=K2=1 (i.e. the chip's factory-trimmed coefficients already match the board's sense
  // resistors -- the same assumption coefficient_by_unit_() already makes for RMS/power).
  double wh_per_pulse =
      static_cast<double>(this->data_.coefficient[ENERGY_AC + channel]) * this->hf_const_ / CSE7761_ENERGY_WH_DIVISOR;
  this->energy_wh_[channel] += static_cast<float>(delta * wh_per_pulse);
}

void CSE7761Component::get_data_() {
  // The effective value of current and voltage Rms is a 24-bit signed number,
  // the highest bit is 0 for valid data,
  //   and when the highest bit is 1, the reading will be processed as zero
  // The active power parameter PowerA/B is in two’s complement format, 32-bit
  // data, the highest bit is Sign bit.
  uint32_t value = this->read_(CSE7761_REG_RMSU, 3);
  this->data_.voltage_rms = (value >= 0x800000) ? 0 : value;

  // Ufreq is a 16-bit unsigned number; the highest bit set means invalid/no signal.
  value = this->read_(CSE7761_REG_UFREQ, 2);
  this->data_.frequency = (value >= 0x8000) ? 0 : value;

  value = this->read_(CSE7761_REG_RMSIA, 3);
  this->data_.current_rms[0] = ((value >= 0x800000) || (value < 1600)) ? 0 : value;  // No load threshold of 10mA
  value = this->read_(CSE7761_REG_POWERPA, 4);
  // PowerPA is two's complement signed 32-bit per datasheet
  this->data_.active_power[0] = (0 == this->data_.current_rms[0]) ? 0 : static_cast<int32_t>(value);

  value = this->read_(CSE7761_REG_RMSIB, 3);
  this->data_.current_rms[1] = ((value >= 0x800000) || (value < 1600)) ? 0 : value;  // No load threshold of 10mA
  value = this->read_(CSE7761_REG_POWERPB, 4);
  // PowerPB is two's complement signed 32-bit per datasheet
  this->data_.active_power[1] = (0 == this->data_.current_rms[1]) ? 0 : static_cast<int32_t>(value);

  // convert values and publish to sensors

  float voltage = static_cast<float>(this->data_.voltage_rms) / this->coefficient_by_unit_(RMS_UC);
  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(voltage);
  }

  // Frequency = system clock / 8 / Ufreq
  float frequency =
      this->data_.frequency ? static_cast<float>(CSE7761_SYSTEM_CLOCK) / 8.0f / this->data_.frequency : 0.0f;
  if (this->frequency_sensor_ != nullptr) {
    this->frequency_sensor_->publish_state(frequency);
  }

  if (this->power_factor_sensor_ != nullptr) {
    // PowerFactor is 24-bit two's complement (sign-extend before converting); 0x7FFFFF = 1.0, 0x800000 = -1.0.
    // With ADC2ON=1 (set in chip_init_()) the chip always reports Channel A here, regardless of any
    // channel-select command, so this is a single value rather than one per channel.
    uint32_t raw_pf = this->read_(CSE7761_REG_POWERFACTOR, 3);
    int32_t pf = (raw_pf & 0x800000) ? static_cast<int32_t>(raw_pf - 0x1000000) : static_cast<int32_t>(raw_pf);
    this->power_factor_sensor_->publish_state(static_cast<float>(pf) / 8388608.0f);  // 0x800000
  }

  for (uint8_t channel = 0; channel < 2; channel++) {
    // Active power = PowerPA * PowerPAC * 1000 / 0x80000000
    float active_power =
        static_cast<float>(this->data_.active_power[channel]) / this->coefficient_by_unit_(POWER_PAC);        // W
    float amps = static_cast<float>(this->data_.current_rms[channel]) / this->coefficient_by_unit_(RMS_IAC);  // A
    ESP_LOGD(TAG, "Channel %d power %f W, current %f A", channel + 1, active_power, amps);
    this->accumulate_energy_(channel, (channel == 0) ? CSE7761_REG_ENERGY_PA : CSE7761_REG_ENERGY_PB);
    if (channel == 0) {
      if (this->power_sensor_1_ != nullptr) {
        this->power_sensor_1_->publish_state(active_power);
      }
      if (this->current_sensor_1_ != nullptr) {
        this->current_sensor_1_->publish_state(amps);
      }
      if (this->energy_sensor_1_ != nullptr) {
        this->energy_sensor_1_->publish_state(this->energy_wh_[0]);
      }
    } else if (channel == 1) {
      if (this->power_sensor_2_ != nullptr) {
        this->power_sensor_2_->publish_state(active_power);
      }
      if (this->current_sensor_2_ != nullptr) {
        this->current_sensor_2_->publish_state(amps);
      }
      if (this->energy_sensor_2_ != nullptr) {
        this->energy_sensor_2_->publish_state(this->energy_wh_[1]);
      }
    }
  }
}

}  // namespace esphome::cse7761

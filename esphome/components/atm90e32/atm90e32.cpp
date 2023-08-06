#include "atm90e32.h"
#include "atm90e32_reg.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace atm90e32 {

static const char *const TAG = "atm90e32";
void ATM90E32Component::loop() {
  const int32_t current_millis = millis();
  if (current_millis - last_periodic_millis < 500)
    return;
  last_periodic_millis = current_millis;
  if (this->phase_[PHASEA].voltage_sensor_ != nullptr) {
    this->phase_[PHASEA].voltage_ = this->get_line_voltage_avg(PHASEA);
  }
  if (this->phase_[PHASEB].voltage_sensor_ != nullptr) {
    this->phase_[PHASEB].voltage_ = this->get_line_voltage_avg(PHASEB);
  }
  if (this->phase_[PHASEC].voltage_sensor_ != nullptr) {
    this->phase_[PHASEC].voltage_ = this->get_line_voltage_avg(PHASEC);
  }
  if (this->phase_[PHASEA].current_sensor_ != nullptr) {
    this->phase_[PHASEA].current_ = this->get_line_current_avg(PHASEA);
  }
  if (this->phase_[PHASEB].current_sensor_ != nullptr) {
    this->phase_[PHASEB].current_ = this->get_line_current_avg(PHASEB);
  }
  if (this->phase_[PHASEC].current_sensor_ != nullptr) {
    this->phase_[PHASEC].current_ = this->get_line_current_avg(PHASEC);
  }
}

void ATM90E32Component::update() {
  if (this->read16_(ATM90E32_REGISTER_METEREN) != 1) {
    this->status_set_warning();
    return;
  }
  for (uint8_t phase = 0; phase < 3; phase++) {
    if (this->phase_[phase].voltage_sensor_ != nullptr) {
      this->phase_[phase].voltage_sensor_->publish_state(this->get_phase_voltage_(phase));
    }
  }
  for (uint8_t phase = 0; phase < 3; phase++) {
    if (this->phase_[phase].current_sensor_ != nullptr) {
      this->phase_[phase].current_sensor_->publish_state(this->get_phase_current_(phase));
    }
  }
  if (this->phase_[PHASEA].power_sensor_ != nullptr) {
    this->phase_[PHASEA].power_sensor_->publish_state(this->get_active_power_a_());
  }
  if (this->phase_[PHASEB].power_sensor_ != nullptr) {
    this->phase_[PHASEB].power_sensor_->publish_state(this->get_active_power_b_());
  }
  if (this->phase_[PHASEC].power_sensor_ != nullptr) {
    this->phase_[PHASEC].power_sensor_->publish_state(this->get_active_power_c_());
  }
  if (this->phase_[PHASEA].reactive_power_sensor_ != nullptr) {
    this->phase_[PHASEA].reactive_power_sensor_->publish_state(this->get_reactive_power_a_());
  }
  if (this->phase_[PHASEB].reactive_power_sensor_ != nullptr) {
    this->phase_[PHASEB].reactive_power_sensor_->publish_state(this->get_reactive_power_b_());
  }
  if (this->phase_[PHASEC].reactive_power_sensor_ != nullptr) {
    this->phase_[PHASEC].reactive_power_sensor_->publish_state(this->get_reactive_power_c_());
  }
  if (this->phase_[PHASEA].power_factor_sensor_ != nullptr) {
    this->phase_[PHASEA].power_factor_sensor_->publish_state(this->get_power_factor_a_());
  }
  if (this->phase_[PHASEB].power_factor_sensor_ != nullptr) {
    this->phase_[PHASEB].power_factor_sensor_->publish_state(this->get_power_factor_b_());
  }
  if (this->phase_[PHASEC].power_factor_sensor_ != nullptr) {
    this->phase_[PHASEC].power_factor_sensor_->publish_state(this->get_power_factor_c_());
  }
  if (this->phase_[PHASEA].forward_active_energy_sensor_ != nullptr) {
    this->phase_[PHASEA].forward_active_energy_sensor_->publish_state(this->get_forward_active_energy_a_());
  }
  if (this->phase_[PHASEB].forward_active_energy_sensor_ != nullptr) {
    this->phase_[PHASEB].forward_active_energy_sensor_->publish_state(this->get_forward_active_energy_b_());
  }
  if (this->phase_[PHASEC].forward_active_energy_sensor_ != nullptr) {
    this->phase_[PHASEC].forward_active_energy_sensor_->publish_state(this->get_forward_active_energy_c_());
  }
  if (this->phase_[PHASEA].reverse_active_energy_sensor_ != nullptr) {
    this->phase_[PHASEA].reverse_active_energy_sensor_->publish_state(this->get_reverse_active_energy_a_());
  }
  if (this->phase_[PHASEB].reverse_active_energy_sensor_ != nullptr) {
    this->phase_[PHASEB].reverse_active_energy_sensor_->publish_state(this->get_reverse_active_energy_b_());
  }
  if (this->phase_[PHASEC].reverse_active_energy_sensor_ != nullptr) {
    this->phase_[PHASEC].reverse_active_energy_sensor_->publish_state(this->get_reverse_active_energy_c_());
  }
  if (this->freq_sensor_ != nullptr) {
    this->freq_sensor_->publish_state(this->get_frequency_());
  }
  if (this->chip_temperature_sensor_ != nullptr) {
    this->chip_temperature_sensor_->publish_state(this->get_chip_temperature_());
  }
  this->status_clear_warning();
}

void ATM90E32Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ATM90E32 Component...");
  this->spi_setup();

  uint16_t mmode0 = 0x87;  // 3P4W 50Hz
  if (line_freq_ == 60) {
    mmode0 |= 1 << 12;  // sets 12th bit to 1, 60Hz
  }

  if (current_phases_ == 2) {
    mmode0 |= 1 << 8;  // sets 8th bit to 1, 3P3W
    mmode0 |= 0 << 1;  // sets 1st bit to 0, phase b is not counted into the all-phase sum energy/power (P/Q/S)
  }

  this->write16_(ATM90E32_REGISTER_SOFTRESET, 0x789A);    // Perform soft reset
  delay(6);                                               // Wait for the minimum 5ms + 1ms
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x55AA);  // enable register config access
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != 0x55AA) {
    ESP_LOGW(TAG, "Could not initialize ATM90E32 IC, check SPI settings");
    this->mark_failed();
    return;
  }

  this->write16_(ATM90E32_REGISTER_METEREN, 0x0001);      // Enable Metering
  this->write16_(ATM90E32_REGISTER_PLCONSTH, 0x0861);   // PL Constant MSB (default) = 140625000
  this->write16_(ATM90E32_REGISTER_PLCONSTL, 0xC468);   // PL Constant LSB (default)
  this->write16_(ATM90E32_REGISTER_ZXCONFIG, 0xD654);   // ZX2, ZX1, ZX0 pin config
  this->write16_(ATM90E32_REGISTER_MMODE0, mmode0);     // Mode Config (frequency set in main program)
  this->write16_(ATM90E32_REGISTER_MMODE1, pga_gain_);  // PGA Gain Configuration for Current Channels
  this->write16_(ATM90E32_REGISTER_PSTARTTH, 0x1D4C);   // All Active Startup Power Threshold - 0.02A/0.00032 = 7500
  this->write16_(ATM90E32_REGISTER_QSTARTTH, 0x1D4C);   // All Reactive Startup Power Threshold - 50%
  this->write16_(ATM90E32_REGISTER_PPHASETH, 0x02EE);   // Each Phase Active Phase Threshold - 0.002A/0.00032 = 750
  this->write16_(ATM90E32_REGISTER_QPHASETH, 0x02EE);   // Each phase Reactive Phase Threshold - 10%
  // Setup voltage and current calibration offsets for PHASE A
  this->phase_[PHASEA].voltage_offset_ = calibrateVoltageOffsetPhase(PHASEA);
  this->write16_(ATM90E32_REGISTER_UOFFSETA, this->phase_[PHASEA].voltage_offset_);  // A Voltage offset
  this->phase_[PHASEA].current_offset_ = calibrateCurrentOffsetPhase(PHASEA);
  this->write16_(ATM90E32_REGISTER_IOFFSETA, this->phase_[PHASEA].current_offset_);  // A Current offset
  // Setup voltage and current gain for PHASE A
  this->write16_(ATM90E32_REGISTER_UGAINA, this->phase_[PHASEA].voltage_gain_);  // A Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINA, this->phase_[PHASEA].ct_gain_);    // A line current gain
  // Setup voltage and current calibration offsets for PHASE B
  this->phase_[PHASEB].voltage_offset_ = calibrateVoltageOffsetPhase(PHASEB);
  this->write16_(ATM90E32_REGISTER_UOFFSETB, this->phase_[PHASEB].voltage_offset_);  // B Voltage offset
  this->phase_[PHASEB].current_offset_ = calibrateCurrentOffsetPhase(PHASEB);
  this->write16_(ATM90E32_REGISTER_IOFFSETB, this->phase_[PHASEB].current_offset_);  // B Current offset
  // Setup voltage and current gain for PHASE B
  this->write16_(ATM90E32_REGISTER_UGAINB, this->phase_[PHASEB].voltage_gain_);  // B Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINB, this->phase_[PHASEB].ct_gain_);    // B line current gain
  // Setup voltage and current calibration offsets for PHASE C
  this->phase_[PHASEC].voltage_offset_ = calibrateVoltageOffsetPhase(PHASEC);
  this->write16_(ATM90E32_REGISTER_UOFFSETC, this->phase_[PHASEC].voltage_offset_);  // C Voltage offset
  this->phase_[PHASEC].current_offset_ = calibrateCurrentOffsetPhase(PHASEC);
  this->write16_(ATM90E32_REGISTER_IOFFSETC, this->phase_[PHASEC].current_offset_);  // C Current offset
  // Setup voltage and current gain for PHASE C
  this->write16_(ATM90E32_REGISTER_UGAINC, this->phase_[PHASEC].voltage_gain_);  // C Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINC, this->phase_[PHASEC].ct_gain_);    // C line current gain
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);                 // end configuration
}

void ATM90E32Component::dump_config() {
  ESP_LOGCONFIG("", "ATM90E32:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ATM90E32 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage A", this->phase_[0].voltage_sensor_);
  LOG_SENSOR("  ", "Current A", this->phase_[0].current_sensor_);
  LOG_SENSOR("  ", "Power A", this->phase_[0].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power A", this->phase_[0].reactive_power_sensor_);
  LOG_SENSOR("  ", "PF A", this->phase_[0].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy A", this->phase_[0].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy A", this->phase_[0].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Voltage B", this->phase_[1].voltage_sensor_);
  LOG_SENSOR("  ", "Current B", this->phase_[1].current_sensor_);
  LOG_SENSOR("  ", "Power B", this->phase_[1].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power B", this->phase_[1].reactive_power_sensor_);
  LOG_SENSOR("  ", "PF B", this->phase_[1].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy B", this->phase_[1].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy B", this->phase_[1].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Voltage C", this->phase_[2].voltage_sensor_);
  LOG_SENSOR("  ", "Current C", this->phase_[2].current_sensor_);
  LOG_SENSOR("  ", "Power C", this->phase_[2].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power C", this->phase_[2].reactive_power_sensor_);
  LOG_SENSOR("  ", "PF C", this->phase_[2].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy C", this->phase_[2].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy C", this->phase_[2].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Frequency", this->freq_sensor_);
  LOG_SENSOR("  ", "Chip Temp", this->chip_temperature_sensor_);
}
float ATM90E32Component::get_setup_priority() const { return setup_priority::DATA; }

uint16_t ATM90E32Component::read16_(uint16_t a_register) {
  uint8_t addrh = (1 << 7) | ((a_register >> 8) & 0x03);
  uint8_t addrl = (a_register & 0xFF);
  uint8_t data[2];
  uint16_t output;
  this->enable();
  delay_microseconds_safe(10);
  this->write_byte(addrh);
  this->write_byte(addrl);
  this->read_array(data, 2);
  this->disable();

  output = (uint16_t(data[0] & 0xFF) << 8) | (data[1] & 0xFF);
  ESP_LOGVV(TAG, "read16_ 0x%04" PRIX16 " output 0x%04" PRIX16, a_register, output);
  return output;
}

int ATM90E32Component::read32_(uint16_t addr_h, uint16_t addr_l) {
  const uint16_t val_h = this->read16_(addr_h);
  const uint16_t val_l = this->read16_(addr_l);
  const int32_t val = (val_h << 16) | val_l;

  ESP_LOGVV(TAG,
            "read32_ addr_h 0x%04" PRIX16 " val_h 0x%04" PRIX16 " addr_l 0x%04" PRIX16 " val_l 0x%04" PRIX16
            " = %" PRId32,
            addr_h, val_h, addr_l, val_l, val);

  return val;
}

void ATM90E32Component::write16_(uint16_t a_register, uint16_t val) {
  ESP_LOGVV(TAG, "write16_ 0x%04" PRIX16 " val 0x%04" PRIX16, a_register, val);
  this->enable();
  this->write_byte16(a_register);
  this->write_byte16(val);
  this->disable();
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != val) ESP_LOGW(TAG, "SPI write error 0x%04X val 0x%04X", a_register, val);
}
float ATM90E32Component::get_phase_voltage_(uint8_t phase) {
  return this->phase_[phase].voltage_;
}
float ATM90E32Component::get_phase_current_(uint8_t phase) {
  return this->phase_[phase].current_;
}
float ATM90E32Component::get_line_voltage_(uint8_t phase) {
  const uint16_t voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != voltage) ESP_LOGW(TAG, "SPI RMS voltage read error");
  return (float) voltage / 100;
}
float ATM90E32Component::get_line_voltage_avg(uint8_t phase) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t voltage = 0;
  for (uint8_t i = 0; i < reads; i++) {
    voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
    if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != voltage) ESP_LOGW(TAG, "SPI RMS voltage read error");
    accumulation += voltage;
  }
  voltage = accumulation / reads;
  this->phase_[phase].voltage_ = (float) voltage / 100;
  return this->phase_[phase].voltage_;
}
float ATM90E32Component::get_line_current_avg(uint8_t phase) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t current = 0;
  for (uint8_t i = 0; i < reads; i++) {
    current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
    if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != current) ESP_LOGW(TAG, "SPI RMS current read error");
    accumulation += current;
  }
  current = accumulation / reads;
  this->phase_[phase].current_ = (float) current / 1000;
  return this->phase_[phase].current_;
}
float ATM90E32Component::get_line_current_(uint8_t phase) {
  const uint16_t current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != current) ESP_LOGW(TAG, "SPI RMS current read error");
  return (float) current / 1000;
}
float ATM90E32Component::get_active_power_a_() {
  const int val = this->read32_(ATM90E32_REGISTER_PMEANA, ATM90E32_REGISTER_PMEANALSB);
  return val * 0.00032f;
}
float ATM90E32Component::get_active_power_b_() {
  const int val = this->read32_(ATM90E32_REGISTER_PMEANB, ATM90E32_REGISTER_PMEANBLSB);
  return val * 0.00032f;
}
float ATM90E32Component::get_active_power_c_() {
  const int val = this->read32_(ATM90E32_REGISTER_PMEANC, ATM90E32_REGISTER_PMEANCLSB);
  return val * 0.00032f;
}
float ATM90E32Component::get_reactive_power_a_() {
  const int val = this->read32_(ATM90E32_REGISTER_QMEANA, ATM90E32_REGISTER_QMEANALSB);
  return val * 0.00032f;
}
float ATM90E32Component::get_reactive_power_b_() {
  const int val = this->read32_(ATM90E32_REGISTER_QMEANB, ATM90E32_REGISTER_QMEANBLSB);
  return val * 0.00032f;
}
float ATM90E32Component::get_reactive_power_c_() {
  const int val = this->read32_(ATM90E32_REGISTER_QMEANC, ATM90E32_REGISTER_QMEANCLSB);
  return val * 0.00032f;
}
float ATM90E32Component::get_power_factor_a_() {
  const int16_t powerfactor = this->read16_(ATM90E32_REGISTER_PFMEANA);
  return (float) powerfactor / 1000;
}
float ATM90E32Component::get_power_factor_b_() {
  const int16_t powerfactor = this->read16_(ATM90E32_REGISTER_PFMEANB);
  return (float) powerfactor / 1000;
}
float ATM90E32Component::get_power_factor_c_() {
  const int16_t powerfactor = this->read16_(ATM90E32_REGISTER_PFMEANC);
  return (float) powerfactor / 1000;
}
float ATM90E32Component::get_forward_active_energy_a_() {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_APENERGYA);
  if ((UINT32_MAX - this->phase_[0].cumulative_forward_active_energy_) > val) {
    this->phase_[0].cumulative_forward_active_energy_ += val;
  } else {
    this->phase_[0].cumulative_forward_active_energy_ = val;
  }
  return ((float) this->phase_[0].cumulative_forward_active_energy_ * 10 / 3200);
}
float ATM90E32Component::get_forward_active_energy_b_() {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_APENERGYB);
  if (UINT32_MAX - this->phase_[1].cumulative_forward_active_energy_ > val) {
    this->phase_[1].cumulative_forward_active_energy_ += val;
  } else {
    this->phase_[1].cumulative_forward_active_energy_ = val;
  }
  return ((float) this->phase_[1].cumulative_forward_active_energy_ * 10 / 3200);
}
float ATM90E32Component::get_forward_active_energy_c_() {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_APENERGYC);
  if (UINT32_MAX - this->phase_[2].cumulative_forward_active_energy_ > val) {
    this->phase_[2].cumulative_forward_active_energy_ += val;
  } else {
    this->phase_[2].cumulative_forward_active_energy_ = val;
  }
  return ((float) this->phase_[2].cumulative_forward_active_energy_ * 10 / 3200);
}
float ATM90E32Component::get_reverse_active_energy_a_() {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_ANENERGYA);
  if (UINT32_MAX - this->phase_[0].cumulative_reverse_active_energy_ > val) {
    this->phase_[0].cumulative_reverse_active_energy_ += val;
  } else {
    this->phase_[0].cumulative_reverse_active_energy_ = val;
  }
  return ((float) this->phase_[0].cumulative_reverse_active_energy_ * 10 / 3200);
}
float ATM90E32Component::get_reverse_active_energy_b_() {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_ANENERGYB);
  if (UINT32_MAX - this->phase_[1].cumulative_reverse_active_energy_ > val) {
    this->phase_[1].cumulative_reverse_active_energy_ += val;
  } else {
    this->phase_[1].cumulative_reverse_active_energy_ = val;
  }
  return ((float) this->phase_[1].cumulative_reverse_active_energy_ * 10 / 3200);
}
float ATM90E32Component::get_reverse_active_energy_c_() {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_ANENERGYC);
  if (UINT32_MAX - this->phase_[2].cumulative_reverse_active_energy_ > val) {
    this->phase_[2].cumulative_reverse_active_energy_ += val;
  } else {
    this->phase_[2].cumulative_reverse_active_energy_ = val;
  }
  return ((float) this->phase_[2].cumulative_reverse_active_energy_ * 10 / 3200);
}
float ATM90E32Component::get_frequency_() {
  const uint16_t freq = this->read16_(ATM90E32_REGISTER_FREQ);
  return (float) freq / 100;
}
float ATM90E32Component::get_chip_temperature_() {
  const uint16_t ctemp = this->read16_(ATM90E32_REGISTER_TEMP);
  return (float) ctemp;
}
}  // namespace atm90e32
}  // namespace esphome

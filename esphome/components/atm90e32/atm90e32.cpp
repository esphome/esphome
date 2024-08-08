#include "atm90e32.h"
#include "atm90e32_reg.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace atm90e32 {

static const char *const TAG = "atm90e32";
void ATM90E32Component::loop() {
  if (this->get_publish_interval_flag_()) {
    this->set_publish_interval_flag_(false);
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].voltage_sensor_ != nullptr) {
        this->phase_[phase].voltage_ = this->get_phase_voltage_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].current_sensor_ != nullptr) {
        this->phase_[phase].current_ = this->get_phase_current_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].power_sensor_ != nullptr) {
        this->phase_[phase].active_power_ = this->get_phase_active_power_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].power_factor_sensor_ != nullptr) {
        this->phase_[phase].power_factor_ = this->get_phase_power_factor_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].reactive_power_sensor_ != nullptr) {
        this->phase_[phase].reactive_power_ = this->get_phase_reactive_power_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].forward_active_energy_sensor_ != nullptr) {
        this->phase_[phase].forward_active_energy_ = this->get_phase_forward_active_energy_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].reverse_active_energy_sensor_ != nullptr) {
        this->phase_[phase].reverse_active_energy_ = this->get_phase_reverse_active_energy_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].phase_angle_sensor_ != nullptr) {
        this->phase_[phase].phase_angle_ = this->get_phase_angle_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].harmonic_active_power_sensor_ != nullptr) {
        this->phase_[phase].harmonic_active_power_ = this->get_phase_harmonic_active_power_(phase);
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].peak_current_sensor_ != nullptr) {
        this->phase_[phase].peak_current_ = this->get_phase_peak_current_(phase);
      }
    }
    // After the local store in collected we can publish them trusting they are withing +-1 haardware sampling
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].voltage_sensor_ != nullptr) {
        this->phase_[phase].voltage_sensor_->publish_state(this->get_local_phase_voltage_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].current_sensor_ != nullptr) {
        this->phase_[phase].current_sensor_->publish_state(this->get_local_phase_current_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].power_sensor_ != nullptr) {
        this->phase_[phase].power_sensor_->publish_state(this->get_local_phase_active_power_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].power_factor_sensor_ != nullptr) {
        this->phase_[phase].power_factor_sensor_->publish_state(this->get_local_phase_power_factor_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].reactive_power_sensor_ != nullptr) {
        this->phase_[phase].reactive_power_sensor_->publish_state(this->get_local_phase_reactive_power_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].forward_active_energy_sensor_ != nullptr) {
        this->phase_[phase].forward_active_energy_sensor_->publish_state(
            this->get_local_phase_forward_active_energy_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].reverse_active_energy_sensor_ != nullptr) {
        this->phase_[phase].reverse_active_energy_sensor_->publish_state(
            this->get_local_phase_reverse_active_energy_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].phase_angle_sensor_ != nullptr) {
        this->phase_[phase].phase_angle_sensor_->publish_state(this->get_local_phase_angle_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].harmonic_active_power_sensor_ != nullptr) {
        this->phase_[phase].harmonic_active_power_sensor_->publish_state(
            this->get_local_phase_harmonic_active_power_(phase));
      }
    }
    for (uint8_t phase = 0; phase < 3; phase++) {
      if (this->phase_[phase].peak_current_sensor_ != nullptr) {
        this->phase_[phase].peak_current_sensor_->publish_state(this->get_local_phase_peak_current_(phase));
      }
    }
    if (this->freq_sensor_ != nullptr) {
      this->freq_sensor_->publish_state(this->get_frequency_());
    }
    if (this->chip_temperature_sensor_ != nullptr) {
      this->chip_temperature_sensor_->publish_state(this->get_chip_temperature_());
    }
  }
}

void ATM90E32Component::update() {
  if (this->read16_(ATM90E32_REGISTER_METEREN) != 1) {
    this->status_set_warning();
    return;
  }
  this->set_publish_interval_flag_(true);
  this->status_clear_warning();
}

void ATM90E32Component::restore_calibrations_() {
  if (enable_offset_calibration_) {
    this->pref_.load(&this->offset_phase_);
  }
};

void ATM90E32Component::run_offset_calibrations() {
  // Run the calibrations and
  // Setup voltage and current calibration offsets for PHASE A
  this->offset_phase_[PHASEA].voltage_offset_ = calibrate_voltage_offset_phase(PHASEA);
  this->phase_[PHASEA].voltage_offset_ = this->offset_phase_[PHASEA].voltage_offset_;
  this->write16_(ATM90E32_REGISTER_UOFFSETA, this->phase_[PHASEA].voltage_offset_);  // C Voltage offset
  this->offset_phase_[PHASEA].current_offset_ = calibrate_current_offset_phase(PHASEA);
  this->phase_[PHASEA].current_offset_ = this->offset_phase_[PHASEA].current_offset_;
  this->write16_(ATM90E32_REGISTER_IOFFSETA, this->phase_[PHASEA].current_offset_);  // C Current offset
  // Setup voltage and current calibration offsets for PHASE B
  this->offset_phase_[PHASEB].voltage_offset_ = calibrate_voltage_offset_phase(PHASEB);
  this->phase_[PHASEB].voltage_offset_ = this->offset_phase_[PHASEB].voltage_offset_;
  this->write16_(ATM90E32_REGISTER_UOFFSETB, this->phase_[PHASEB].voltage_offset_);  // C Voltage offset
  this->offset_phase_[PHASEB].current_offset_ = calibrate_current_offset_phase(PHASEB);
  this->phase_[PHASEB].current_offset_ = this->offset_phase_[PHASEB].current_offset_;
  this->write16_(ATM90E32_REGISTER_IOFFSETB, this->phase_[PHASEB].current_offset_);  // C Current offset
  // Setup voltage and current calibration offsets for PHASE C
  this->offset_phase_[PHASEC].voltage_offset_ = calibrate_voltage_offset_phase(PHASEC);
  this->phase_[PHASEC].voltage_offset_ = this->offset_phase_[PHASEC].voltage_offset_;
  this->write16_(ATM90E32_REGISTER_UOFFSETC, this->phase_[PHASEC].voltage_offset_);  // C Voltage offset
  this->offset_phase_[PHASEC].current_offset_ = calibrate_current_offset_phase(PHASEC);
  this->phase_[PHASEC].current_offset_ = this->offset_phase_[PHASEC].current_offset_;
  this->write16_(ATM90E32_REGISTER_IOFFSETC, this->phase_[PHASEC].current_offset_);  // C Current offset
  this->pref_.save(&this->offset_phase_);
  ESP_LOGI(TAG, "PhaseA Vo=%5d PhaseB Vo=%5d PhaseC Vo=%5d", this->offset_phase_[PHASEA].voltage_offset_,
           this->offset_phase_[PHASEB].voltage_offset_, this->offset_phase_[PHASEC].voltage_offset_);
  ESP_LOGI(TAG, "PhaseA Io=%5d PhaseB Io=%5d PhaseC Io=%5d", this->offset_phase_[PHASEA].current_offset_,
           this->offset_phase_[PHASEB].current_offset_, this->offset_phase_[PHASEC].current_offset_);
}

void ATM90E32Component::clear_offset_calibrations() {
  // Clear the calibrations and
  this->offset_phase_[PHASEA].voltage_offset_ = 0;
  this->phase_[PHASEA].voltage_offset_ = this->offset_phase_[PHASEA].voltage_offset_;
  this->write16_(ATM90E32_REGISTER_UOFFSETA, this->phase_[PHASEA].voltage_offset_);  // C Voltage offset
  this->offset_phase_[PHASEA].current_offset_ = 0;
  this->phase_[PHASEA].current_offset_ = this->offset_phase_[PHASEA].current_offset_;
  this->write16_(ATM90E32_REGISTER_IOFFSETA, this->phase_[PHASEA].current_offset_);  // C Current offset
  this->offset_phase_[PHASEB].voltage_offset_ = 0;
  this->phase_[PHASEB].voltage_offset_ = this->offset_phase_[PHASEB].voltage_offset_;
  this->write16_(ATM90E32_REGISTER_UOFFSETB, this->phase_[PHASEB].voltage_offset_);  // C Voltage offset
  this->offset_phase_[PHASEB].current_offset_ = 0;
  this->phase_[PHASEB].current_offset_ = this->offset_phase_[PHASEB].current_offset_;
  this->write16_(ATM90E32_REGISTER_IOFFSETB, this->phase_[PHASEB].current_offset_);  // C Current offset
  this->offset_phase_[PHASEC].voltage_offset_ = 0;
  this->phase_[PHASEC].voltage_offset_ = this->offset_phase_[PHASEC].voltage_offset_;
  this->write16_(ATM90E32_REGISTER_UOFFSETC, this->phase_[PHASEC].voltage_offset_);  // C Voltage offset
  this->offset_phase_[PHASEC].current_offset_ = 0;
  this->phase_[PHASEC].current_offset_ = this->offset_phase_[PHASEC].current_offset_;
  this->write16_(ATM90E32_REGISTER_IOFFSETC, this->phase_[PHASEC].current_offset_);  // C Current offset
  this->pref_.save(&this->offset_phase_);
  ESP_LOGI(TAG, "PhaseA Vo=%5d PhaseB Vo=%5d PhaseC Vo=%5d", this->offset_phase_[PHASEA].voltage_offset_,
           this->offset_phase_[PHASEB].voltage_offset_, this->offset_phase_[PHASEC].voltage_offset_);
  ESP_LOGI(TAG, "PhaseA Io=%5d PhaseB Io=%5d PhaseC Io=%5d", this->offset_phase_[PHASEA].current_offset_,
           this->offset_phase_[PHASEB].current_offset_, this->offset_phase_[PHASEC].current_offset_);
}

void ATM90E32Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ATM90E32 Component...");
  this->spi_setup();
  if (this->enable_offset_calibration_) {
    uint32_t hash = fnv1_hash(App.get_friendly_name());
    this->pref_ = global_preferences->make_preference<Calibration[3]>(hash, true);
    this->restore_calibrations_();
  }
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

  this->write16_(ATM90E32_REGISTER_METEREN, 0x0001);        // Enable Metering
  this->write16_(ATM90E32_REGISTER_SAGPEAKDETCFG, 0xFF3F);  // Peak Detector time ms (15:8), Sag Period ms (7:0)
  this->write16_(ATM90E32_REGISTER_PLCONSTH, 0x0861);       // PL Constant MSB (default) = 140625000
  this->write16_(ATM90E32_REGISTER_PLCONSTL, 0xC468);       // PL Constant LSB (default)
  this->write16_(ATM90E32_REGISTER_ZXCONFIG, 0xD654);       // ZX2, ZX1, ZX0 pin config
  this->write16_(ATM90E32_REGISTER_MMODE0, mmode0);         // Mode Config (frequency set in main program)
  this->write16_(ATM90E32_REGISTER_MMODE1, pga_gain_);      // PGA Gain Configuration for Current Channels
  this->write16_(ATM90E32_REGISTER_PSTARTTH, 0x1D4C);       // All Active Startup Power Threshold - 0.02A/0.00032 = 7500
  this->write16_(ATM90E32_REGISTER_QSTARTTH, 0x1D4C);       // All Reactive Startup Power Threshold - 50%
  this->write16_(ATM90E32_REGISTER_SSTARTTH, 0x1D4C);       // All Reactive Startup Power Threshold - 50%
  this->write16_(ATM90E32_REGISTER_PPHASETH, 0x02EE);       // Each Phase Active Phase Threshold - 0.002A/0.00032 = 750
  this->write16_(ATM90E32_REGISTER_QPHASETH, 0x02EE);       // Each phase Reactive Phase Threshold - 10%
  // Setup voltage and current gain for PHASE A
  this->write16_(ATM90E32_REGISTER_UGAINA, this->phase_[PHASEA].voltage_gain_);  // A Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINA, this->phase_[PHASEA].ct_gain_);       // A line current gain
  // Setup voltage and current gain for PHASE B
  this->write16_(ATM90E32_REGISTER_UGAINB, this->phase_[PHASEB].voltage_gain_);  // B Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINB, this->phase_[PHASEB].ct_gain_);       // B line current gain
  // Setup voltage and current gain for PHASE C
  this->write16_(ATM90E32_REGISTER_UGAINC, this->phase_[PHASEC].voltage_gain_);  // C Voltage rms gain
  this->write16_(ATM90E32_REGISTER_IGAINC, this->phase_[PHASEC].ct_gain_);       // C line current gain
  this->write16_(ATM90E32_REGISTER_CFGREGACCEN, 0x0000);                         // end configuration
}

void ATM90E32Component::dump_config() {
  ESP_LOGCONFIG("", "ATM90E32:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ATM90E32 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage A", this->phase_[PHASEA].voltage_sensor_);
  LOG_SENSOR("  ", "Current A", this->phase_[PHASEA].current_sensor_);
  LOG_SENSOR("  ", "Power A", this->phase_[PHASEA].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power A", this->phase_[PHASEA].reactive_power_sensor_);
  LOG_SENSOR("  ", "PF A", this->phase_[PHASEA].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy A", this->phase_[PHASEA].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy A", this->phase_[PHASEA].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power A", this->phase_[PHASEA].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle A", this->phase_[PHASEA].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current A", this->phase_[PHASEA].peak_current_sensor_);
  LOG_SENSOR("  ", "Voltage B", this->phase_[PHASEB].voltage_sensor_);
  LOG_SENSOR("  ", "Current B", this->phase_[PHASEB].current_sensor_);
  LOG_SENSOR("  ", "Power B", this->phase_[PHASEB].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power B", this->phase_[PHASEB].reactive_power_sensor_);
  LOG_SENSOR("  ", "PF B", this->phase_[PHASEB].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy B", this->phase_[PHASEB].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy B", this->phase_[PHASEB].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power A", this->phase_[PHASEB].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle A", this->phase_[PHASEB].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current A", this->phase_[PHASEB].peak_current_sensor_);
  LOG_SENSOR("  ", "Voltage C", this->phase_[PHASEC].voltage_sensor_);
  LOG_SENSOR("  ", "Current C", this->phase_[PHASEC].current_sensor_);
  LOG_SENSOR("  ", "Power C", this->phase_[PHASEC].power_sensor_);
  LOG_SENSOR("  ", "Reactive Power C", this->phase_[PHASEC].reactive_power_sensor_);
  LOG_SENSOR("  ", "PF C", this->phase_[PHASEC].power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy C", this->phase_[PHASEC].forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy C", this->phase_[PHASEC].reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Harmonic Power A", this->phase_[PHASEC].harmonic_active_power_sensor_);
  LOG_SENSOR("  ", "Phase Angle A", this->phase_[PHASEC].phase_angle_sensor_);
  LOG_SENSOR("  ", "Peak Current A", this->phase_[PHASEC].peak_current_sensor_);
  LOG_SENSOR("  ", "Frequency", this->freq_sensor_);
  LOG_SENSOR("  ", "Chip Temp", this->chip_temperature_sensor_);
}

float ATM90E32Component::get_setup_priority() const { return setup_priority::IO; }

// R/C registers can conly be cleared after the LastSPIData register is updated (register 78H)
// Peakdetect period: 05H. Bit 15:8 are PeakDet_period in ms. 7:0 are Sag_period
// Default is 143FH (20ms, 63ms)
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
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != val)
    ESP_LOGW(TAG, "SPI write error 0x%04X val 0x%04X", a_register, val);
}

float ATM90E32Component::get_local_phase_voltage_(uint8_t phase) { return this->phase_[phase].voltage_; }

float ATM90E32Component::get_local_phase_current_(uint8_t phase) { return this->phase_[phase].current_; }

float ATM90E32Component::get_local_phase_active_power_(uint8_t phase) { return this->phase_[phase].active_power_; }

float ATM90E32Component::get_local_phase_reactive_power_(uint8_t phase) { return this->phase_[phase].reactive_power_; }

float ATM90E32Component::get_local_phase_power_factor_(uint8_t phase) { return this->phase_[phase].power_factor_; }

float ATM90E32Component::get_local_phase_forward_active_energy_(uint8_t phase) {
  return this->phase_[phase].forward_active_energy_;
}

float ATM90E32Component::get_local_phase_reverse_active_energy_(uint8_t phase) {
  return this->phase_[phase].reverse_active_energy_;
}

float ATM90E32Component::get_local_phase_angle_(uint8_t phase) { return this->phase_[phase].phase_angle_; }

float ATM90E32Component::get_local_phase_harmonic_active_power_(uint8_t phase) {
  return this->phase_[phase].harmonic_active_power_;
}

float ATM90E32Component::get_local_phase_peak_current_(uint8_t phase) { return this->phase_[phase].peak_current_; }

float ATM90E32Component::get_phase_voltage_(uint8_t phase) {
  const uint16_t voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != voltage)
    ESP_LOGW(TAG, "SPI URMS voltage register read error.");
  return (float) voltage / 100;
}

float ATM90E32Component::get_phase_voltage_avg_(uint8_t phase) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t voltage = 0;
  for (uint8_t i = 0; i < reads; i++) {
    voltage = this->read16_(ATM90E32_REGISTER_URMS + phase);
    if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != voltage)
      ESP_LOGW(TAG, "SPI URMS voltage register read error.");
    accumulation += voltage;
  }
  voltage = accumulation / reads;
  this->phase_[phase].voltage_ = (float) voltage / 100;
  return this->phase_[phase].voltage_;
}

float ATM90E32Component::get_phase_current_avg_(uint8_t phase) {
  const uint8_t reads = 10;
  uint32_t accumulation = 0;
  uint16_t current = 0;
  for (uint8_t i = 0; i < reads; i++) {
    current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
    if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != current)
      ESP_LOGW(TAG, "SPI IRMS current register read error.");
    accumulation += current;
  }
  current = accumulation / reads;
  this->phase_[phase].current_ = (float) current / 1000;
  return this->phase_[phase].current_;
}

float ATM90E32Component::get_phase_current_(uint8_t phase) {
  const uint16_t current = this->read16_(ATM90E32_REGISTER_IRMS + phase);
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != current)
    ESP_LOGW(TAG, "SPI IRMS current register read error.");
  return (float) current / 1000;
}

float ATM90E32Component::get_phase_active_power_(uint8_t phase) {
  const int val = this->read32_(ATM90E32_REGISTER_PMEAN + phase, ATM90E32_REGISTER_PMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_reactive_power_(uint8_t phase) {
  const int val = this->read32_(ATM90E32_REGISTER_QMEAN + phase, ATM90E32_REGISTER_QMEANLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_power_factor_(uint8_t phase) {
  const int16_t powerfactor = this->read16_(ATM90E32_REGISTER_PFMEAN + phase);
  if (this->read16_(ATM90E32_REGISTER_LASTSPIDATA) != powerfactor)
    ESP_LOGW(TAG, "SPI power factor read error.");
  return (float) powerfactor / 1000;
}

float ATM90E32Component::get_phase_forward_active_energy_(uint8_t phase) {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_APENERGY + phase);
  if ((UINT32_MAX - this->phase_[phase].cumulative_forward_active_energy_) > val) {
    this->phase_[phase].cumulative_forward_active_energy_ += val;
  } else {
    this->phase_[phase].cumulative_forward_active_energy_ = val;
  }
  return ((float) this->phase_[phase].cumulative_forward_active_energy_ * 10 / 3200);
}

float ATM90E32Component::get_phase_reverse_active_energy_(uint8_t phase) {
  const uint16_t val = this->read16_(ATM90E32_REGISTER_ANENERGY);
  if (UINT32_MAX - this->phase_[phase].cumulative_reverse_active_energy_ > val) {
    this->phase_[phase].cumulative_reverse_active_energy_ += val;
  } else {
    this->phase_[phase].cumulative_reverse_active_energy_ = val;
  }
  return ((float) this->phase_[phase].cumulative_reverse_active_energy_ * 10 / 3200);
}

float ATM90E32Component::get_phase_harmonic_active_power_(uint8_t phase) {
  int val = this->read32_(ATM90E32_REGISTER_PMEANH + phase, ATM90E32_REGISTER_PMEANHLSB + phase);
  return val * 0.00032f;
}

float ATM90E32Component::get_phase_angle_(uint8_t phase) {
  uint16_t val = this->read16_(ATM90E32_REGISTER_PANGLE + phase) / 10.0;
  return (float) (val > 180) ? val - 360.0 : val;
}

float ATM90E32Component::get_phase_peak_current_(uint8_t phase) {
  int16_t val = (float) this->read16_(ATM90E32_REGISTER_IPEAK + phase);
  if (!this->peak_current_signed_)
    val = abs(val);
  // phase register * phase current gain value  / 1000 * 2^13
  return (float) (val * this->phase_[phase].ct_gain_ / 8192000.0);
}

float ATM90E32Component::get_frequency_() {
  const uint16_t freq = this->read16_(ATM90E32_REGISTER_FREQ);
  return (float) freq / 100;
}

float ATM90E32Component::get_chip_temperature_() {
  const uint16_t ctemp = this->read16_(ATM90E32_REGISTER_TEMP);
  return (float) ctemp;
}

uint16_t ATM90E32Component::calibrate_voltage_offset_phase(uint8_t phase) {
  const uint8_t num_reads = 5;
  uint64_t total_value = 0;
  for (int i = 0; i < num_reads; ++i) {
    const uint32_t measurement_value = read32_(ATM90E32_REGISTER_URMS + phase, ATM90E32_REGISTER_URMSLSB + phase);
    total_value += measurement_value;
  }
  const uint32_t average_value = total_value / num_reads;
  const uint32_t shifted_value = average_value >> 7;
  const uint32_t voltage_offset = ~shifted_value + 1;
  return voltage_offset & 0xFFFF;  // Take the lower 16 bits
}

uint16_t ATM90E32Component::calibrate_current_offset_phase(uint8_t phase) {
  const uint8_t num_reads = 5;
  uint64_t total_value = 0;
  for (int i = 0; i < num_reads; ++i) {
    const uint32_t measurement_value = read32_(ATM90E32_REGISTER_IRMS + phase, ATM90E32_REGISTER_IRMSLSB + phase);
    total_value += measurement_value;
  }
  const uint32_t average_value = total_value / num_reads;
  const uint32_t current_offset = ~average_value + 1;
  return current_offset & 0xFFFF;  // Take the lower 16 bits
}

}  // namespace atm90e32
}  // namespace esphome

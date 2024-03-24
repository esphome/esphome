#include "atm90e26.h"
#include "atm90e26_reg.h"
#include "esphome/core/log.h"

namespace esphome {
namespace atm90e26 {

static const char *const TAG = "atm90e26";

void ATM90E26Component::update() {
  if (this->read16_(ATM90E26_REGISTER_FUNCEN) != 0x0030) {
    this->status_set_warning();
    return;
  }

  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state(this->get_line_voltage_());
  }
  if (this->current_sensor_ != nullptr) {
    this->current_sensor_->publish_state(this->get_line_current_());
  }
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->publish_state(this->get_active_power_());
  }
  if (this->reactive_power_sensor_ != nullptr) {
    this->reactive_power_sensor_->publish_state(this->get_reactive_power_());
  }
  if (this->power_factor_sensor_ != nullptr) {
    this->power_factor_sensor_->publish_state(this->get_power_factor_());
  }
  if (this->forward_active_energy_sensor_ != nullptr) {
    this->forward_active_energy_sensor_->publish_state(this->get_forward_active_energy_());
  }
  if (this->reverse_active_energy_sensor_ != nullptr) {
    this->reverse_active_energy_sensor_->publish_state(this->get_reverse_active_energy_());
  }
  if (this->freq_sensor_ != nullptr) {
    this->freq_sensor_->publish_state(this->get_frequency_());
  }
  this->status_clear_warning();
}

void ATM90E26Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ATM90E26 Component...");
  this->spi_setup();

  uint16_t mmode = 0x422;  // default values for everything but L/N line current gains
  mmode |= (gain_pga_ & 0x7) << 13;
  mmode |= (n_line_gain_ & 0x3) << 11;

  this->write16_(ATM90E26_REGISTER_SOFTRESET, 0x789A);  // Perform soft reset
  this->write16_(ATM90E26_REGISTER_FUNCEN,
                 0x0030);  // Voltage sag irq=1, report on warnout pin=1, energy dir change irq=0
  uint16_t read = this->read16_(ATM90E26_REGISTER_LASTDATA);
  if (read != 0x0030) {
    ESP_LOGW(TAG, "Could not initialize ATM90E26 IC, check SPI settings: %d", read);
    this->mark_failed();
    return;
  }
  // TODO: 100 * <nominal voltage, e.g. 230> * sqrt(2) * <fraction of nominal, e.g. 0.9> / (4 * gain_voltage/32768)
  this->write16_(ATM90E26_REGISTER_SAGTH, 0x17DD);  // Voltage sag threshhold 0x1F2F

  // Set metering calibration values
  this->write16_(ATM90E26_REGISTER_CALSTART, 0x5678);  // CAL Metering calibration startup command

  // Configure
  this->write16_(ATM90E26_REGISTER_MMODE, mmode);  // Metering Mode Configuration (see above)

  this->write16_(ATM90E26_REGISTER_PLCONSTH, (pl_const_ >> 16));   // PL Constant MSB
  this->write16_(ATM90E26_REGISTER_PLCONSTL, pl_const_ & 0xFFFF);  // PL Constant LSB

  // Calibrate this to be 1 pulse per Wh
  this->write16_(ATM90E26_REGISTER_LGAIN, gain_metering_);  // L Line Calibration Gain (active power metering)
  this->write16_(ATM90E26_REGISTER_LPHI, 0x0000);           // L Line Calibration Angle
  this->write16_(ATM90E26_REGISTER_NGAIN, 0x0000);          // N Line Calibration Gain
  this->write16_(ATM90E26_REGISTER_NPHI, 0x0000);           // N Line Calibration Angle
  this->write16_(ATM90E26_REGISTER_PSTARTTH, 0x08BD);       // Active Startup Power Threshold (default) = 2237
  this->write16_(ATM90E26_REGISTER_PNOLTH, 0x0000);         // Active No-Load Power Threshold
  this->write16_(ATM90E26_REGISTER_QSTARTTH, 0x0AEC);       // Reactive Startup Power Threshold (default) = 2796
  this->write16_(ATM90E26_REGISTER_QNOLTH, 0x0000);         // Reactive No-Load Power Threshold

  // Compute Checksum for the registers we set above
  // low byte = sum of all bytes
  uint16_t cs =
      ((mmode >> 8) + (mmode & 0xFF) + (pl_const_ >> 24) + ((pl_const_ >> 16) & 0xFF) + ((pl_const_ >> 8) & 0xFF) +
       (pl_const_ & 0xFF) + (gain_metering_ >> 8) + (gain_metering_ & 0xFF) + 0x08 + 0xBD + 0x0A + 0xEC) &
      0xFF;
  // high byte = XOR of all bytes
  cs |= ((mmode >> 8) ^ (mmode & 0xFF) ^ (pl_const_ >> 24) ^ ((pl_const_ >> 16) & 0xFF) ^ ((pl_const_ >> 8) & 0xFF) ^
         (pl_const_ & 0xFF) ^ (gain_metering_ >> 8) ^ (gain_metering_ & 0xFF) ^ 0x08 ^ 0xBD ^ 0x0A ^ 0xEC)
        << 8;

  this->write16_(ATM90E26_REGISTER_CS1, cs);
  ESP_LOGVV(TAG, "Set CS1 to: 0x%04X", cs);

  // Set measurement calibration values
  this->write16_(ATM90E26_REGISTER_ADJSTART, 0x5678);      // Measurement calibration startup command, registers 31-3A
  this->write16_(ATM90E26_REGISTER_UGAIN, gain_voltage_);  // Voltage RMS gain
  this->write16_(ATM90E26_REGISTER_IGAINL, gain_ct_);      // L line current RMS gain
  this->write16_(ATM90E26_REGISTER_IGAINN, 0x7530);        // N Line Current RMS Gain
  this->write16_(ATM90E26_REGISTER_UOFFSET, 0x0000);       // Voltage Offset
  this->write16_(ATM90E26_REGISTER_IOFFSETL, 0x0000);      // L Line Current Offset
  this->write16_(ATM90E26_REGISTER_IOFFSETN, 0x0000);      // N Line Current Offse
  this->write16_(ATM90E26_REGISTER_POFFSETL, 0x0000);      // L Line Active Power Offset
  this->write16_(ATM90E26_REGISTER_QOFFSETL, 0x0000);      // L Line Reactive Power Offset
  this->write16_(ATM90E26_REGISTER_POFFSETN, 0x0000);      // N Line Active Power Offset
  this->write16_(ATM90E26_REGISTER_QOFFSETN, 0x0000);      // N Line Reactive Power Offset

  // Compute Checksum for the registers we set above
  cs = ((gain_voltage_ >> 8) + (gain_voltage_ & 0xFF) + (gain_ct_ >> 8) + (gain_ct_ & 0xFF) + 0x75 + 0x30) & 0xFF;
  cs |= ((gain_voltage_ >> 8) ^ (gain_voltage_ & 0xFF) ^ (gain_ct_ >> 8) ^ (gain_ct_ & 0xFF) ^ 0x75 ^ 0x30) << 8;
  this->write16_(ATM90E26_REGISTER_CS2, cs);
  ESP_LOGVV(TAG, "Set CS2 to: 0x%04X", cs);

  this->write16_(ATM90E26_REGISTER_CALSTART,
                 0x8765);  // Checks correctness of 21-2B registers and starts normal metering if ok
  this->write16_(ATM90E26_REGISTER_ADJSTART,
                 0x8765);  // Checks correctness of 31-3A registers and starts normal measurement  if ok

  const uint16_t sys_status = this->read16_(ATM90E26_REGISTER_SYSSTATUS);
  if (sys_status & 0xC000) {  // Checksum 1 Error

    ESP_LOGW(TAG, "Could not initialize ATM90E26 IC: CS1 was incorrect, expected: 0x%04X",
             this->read16_(ATM90E26_REGISTER_CS1));
    this->mark_failed();
  }
  if (sys_status & 0x3000) {  // Checksum 2 Error
    ESP_LOGW(TAG, "Could not initialize ATM90E26 IC: CS2 was incorrect, expected: 0x%04X",
             this->read16_(ATM90E26_REGISTER_CS2));
    this->mark_failed();
  }
}

void ATM90E26Component::dump_config() {
  ESP_LOGCONFIG("", "ATM90E26:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with ATM90E26 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage A", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current A", this->current_sensor_);
  LOG_SENSOR("  ", "Power A", this->power_sensor_);
  LOG_SENSOR("  ", "Reactive Power A", this->reactive_power_sensor_);
  LOG_SENSOR("  ", "PF A", this->power_factor_sensor_);
  LOG_SENSOR("  ", "Active Forward Energy A", this->forward_active_energy_sensor_);
  LOG_SENSOR("  ", "Active Reverse Energy A", this->reverse_active_energy_sensor_);
  LOG_SENSOR("  ", "Frequency", this->freq_sensor_);
}
float ATM90E26Component::get_setup_priority() const { return setup_priority::DATA; }

uint16_t ATM90E26Component::read16_(uint8_t a_register) {
  uint8_t data[2];
  uint16_t output;

  this->enable();
  delayMicroseconds(4);
  this->write_byte(a_register | 0x80);
  delayMicroseconds(4);
  this->read_array(data, 2);
  this->disable();

  output = (uint16_t(data[0] & 0xFF) << 8) | (data[1] & 0xFF);
  ESP_LOGVV(TAG, "read16_ 0x%04X output 0x%04X", a_register, output);
  return output;
}

void ATM90E26Component::write16_(uint8_t a_register, uint16_t val) {
  ESP_LOGVV(TAG, "write16_ 0x%04X val 0x%04X", a_register, val);
  this->enable();
  delayMicroseconds(4);
  this->write_byte(a_register & 0x7F);
  delayMicroseconds(4);
  this->write_byte((val >> 8) & 0xFF);
  this->write_byte(val & 0xFF);
  this->disable();
}

float ATM90E26Component::get_line_current_() {
  const uint16_t current = this->read16_(ATM90E26_REGISTER_IRMS);
  return current / 1000.0f;
}

float ATM90E26Component::get_line_voltage_() {
  const uint16_t voltage = this->read16_(ATM90E26_REGISTER_URMS);
  return voltage / 100.0f;
}

float ATM90E26Component::get_active_power_() {
  const int16_t val = this->read16_(ATM90E26_REGISTER_PMEAN);  // two's complement
  return (float) val;
}

float ATM90E26Component::get_reactive_power_() {
  const int16_t val = this->read16_(ATM90E26_REGISTER_QMEAN);  // two's complement
  return (float) val;
}

float ATM90E26Component::get_power_factor_() {
  const uint16_t val = this->read16_(ATM90E26_REGISTER_POWERF);  // signed
  if (val & 0x8000) {
    return -(val & 0x7FF) / 1000.0f;
  } else {
    return val / 1000.0f;
  }
}

float ATM90E26Component::get_forward_active_energy_() {
  const uint16_t val = this->read16_(ATM90E26_REGISTER_APENERGY);
  if ((UINT32_MAX - this->cumulative_forward_active_energy_) > val) {
    this->cumulative_forward_active_energy_ += val;
  } else {
    this->cumulative_forward_active_energy_ = val;
  }
  // The register holds thenths of pulses, we want to output Wh
  return (this->cumulative_forward_active_energy_ * 100.0f / meter_constant_);
}

float ATM90E26Component::get_reverse_active_energy_() {
  const uint16_t val = this->read16_(ATM90E26_REGISTER_ANENERGY);
  if (UINT32_MAX - this->cumulative_reverse_active_energy_ > val) {
    this->cumulative_reverse_active_energy_ += val;
  } else {
    this->cumulative_reverse_active_energy_ = val;
  }
  return (this->cumulative_reverse_active_energy_ * 100.0f / meter_constant_);
}

float ATM90E26Component::get_frequency_() {
  const uint16_t freq = this->read16_(ATM90E26_REGISTER_FREQ);
  return freq / 100.0f;
}

}  // namespace atm90e26
}  // namespace esphome

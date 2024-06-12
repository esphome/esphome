// This component was developed using knowledge gathered by a number
// of people who reverse-engineered the Shelly 3EM:
//
// @AndreKR on GitHub
// Axel (@Axel830 on GitHub)
// Marko (@goodkiller on GitHub)
// Michaël Piron (@michaelpiron on GitHub)
// Theo Arends (@arendst on GitHub)

#include "ade7880.h"
#include "ade7880_registers.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace ade7880 {

static const char *const TAG = "ade7880";

void IRAM_ATTR ADE7880Store::gpio_intr(ADE7880Store *arg) { arg->reset_done = true; }

void ADE7880::setup() {
  if (this->irq0_pin_ != nullptr) {
    this->irq0_pin_->setup();
  }
  this->irq1_pin_->setup();
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
  }
  this->store_.irq1_pin = this->irq1_pin_->to_isr();
  this->irq1_pin_->attach_interrupt(ADE7880Store::gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);

  // if IRQ1 is already asserted, the cause must be determined
  if (this->irq1_pin_->digital_read() == 0) {
    ESP_LOGD(TAG, "IRQ1 found asserted during setup()");
    auto status1 = read_u32_register16_(STATUS1);
    if ((status1 & ~STATUS1_RSTDONE) != 0) {
      // not safe to proceed, must initiate reset
      ESP_LOGD(TAG, "IRQ1 asserted for !RSTDONE, resetting device");
      this->reset_device_();
      return;
    }
    if ((status1 & STATUS1_RSTDONE) == STATUS1_RSTDONE) {
      // safe to proceed, device has just completed reset cycle
      ESP_LOGD(TAG, "Acknowledging RSTDONE");
      this->write_u32_register16_(STATUS0, 0xFFFF);
      this->write_u32_register16_(STATUS1, 0xFFFF);
      this->init_device_();
      return;
    }
  }

  this->reset_device_();
}

void ADE7880::loop() {
  // check for completion of a reset cycle
  if (!this->store_.reset_done) {
    return;
  }

  ESP_LOGD(TAG, "Acknowledging RSTDONE");
  this->write_u32_register16_(STATUS0, 0xFFFF);
  this->write_u32_register16_(STATUS1, 0xFFFF);
  this->init_device_();
  this->store_.reset_done = false;
  this->store_.reset_pending = false;
}

template<typename F>
void ADE7880::update_sensor_from_s24zp_register16_(sensor::Sensor *sensor, uint16_t a_register, F &&f) {
  if (sensor == nullptr) {
    return;
  }

  float val = this->read_s24zp_register16_(a_register);
  sensor->publish_state(f(val));
}

template<typename F>
void ADE7880::update_sensor_from_s16_register16_(sensor::Sensor *sensor, uint16_t a_register, F &&f) {
  if (sensor == nullptr) {
    return;
  }

  float val = this->read_s16_register16_(a_register);
  sensor->publish_state(f(val));
}

template<typename F>
void ADE7880::update_sensor_from_s32_register16_(sensor::Sensor *sensor, uint16_t a_register, F &&f) {
  if (sensor == nullptr) {
    return;
  }

  float val = this->read_s32_register16_(a_register);
  sensor->publish_state(f(val));
}

void ADE7880::update() {
  if (this->store_.reset_pending) {
    return;
  }

  auto start = millis();

  if (this->channel_n_ != nullptr) {
    auto *chan = this->channel_n_;
    this->update_sensor_from_s24zp_register16_(chan->current, NIRMS, [](float val) { return val / 100000.0f; });
  }

  if (this->channel_a_ != nullptr) {
    auto *chan = this->channel_a_;
    this->update_sensor_from_s24zp_register16_(chan->current, AIRMS, [](float val) { return val / 100000.0f; });
    this->update_sensor_from_s24zp_register16_(chan->voltage, BVRMS, [](float val) { return val / 10000.0f; });
    this->update_sensor_from_s24zp_register16_(chan->active_power, AWATT, [](float val) { return val / 100.0f; });
    this->update_sensor_from_s24zp_register16_(chan->apparent_power, AVA, [](float val) { return val / 100.0f; });
    this->update_sensor_from_s16_register16_(chan->power_factor, APF,
                                             [](float val) { return std::abs(val / -327.68f); });
    this->update_sensor_from_s32_register16_(chan->forward_active_energy, AFWATTHR, [&chan](float val) {
      return chan->forward_active_energy_total += val / 14400.0f;
    });
    this->update_sensor_from_s32_register16_(chan->reverse_active_energy, AFWATTHR, [&chan](float val) {
      return chan->reverse_active_energy_total += val / 14400.0f;
    });
  }

  if (this->channel_b_ != nullptr) {
    auto *chan = this->channel_b_;
    this->update_sensor_from_s24zp_register16_(chan->current, BIRMS, [](float val) { return val / 100000.0f; });
    this->update_sensor_from_s24zp_register16_(chan->voltage, BVRMS, [](float val) { return val / 10000.0f; });
    this->update_sensor_from_s24zp_register16_(chan->active_power, BWATT, [](float val) { return val / 100.0f; });
    this->update_sensor_from_s24zp_register16_(chan->apparent_power, BVA, [](float val) { return val / 100.0f; });
    this->update_sensor_from_s16_register16_(chan->power_factor, BPF,
                                             [](float val) { return std::abs(val / -327.68f); });
    this->update_sensor_from_s32_register16_(chan->forward_active_energy, BFWATTHR, [&chan](float val) {
      return chan->forward_active_energy_total += val / 14400.0f;
    });
    this->update_sensor_from_s32_register16_(chan->reverse_active_energy, BFWATTHR, [&chan](float val) {
      return chan->reverse_active_energy_total += val / 14400.0f;
    });
  }

  if (this->channel_c_ != nullptr) {
    auto *chan = this->channel_c_;
    this->update_sensor_from_s24zp_register16_(chan->current, CIRMS, [](float val) { return val / 100000.0f; });
    this->update_sensor_from_s24zp_register16_(chan->voltage, CVRMS, [](float val) { return val / 10000.0f; });
    this->update_sensor_from_s24zp_register16_(chan->active_power, CWATT, [](float val) { return val / 100.0f; });
    this->update_sensor_from_s24zp_register16_(chan->apparent_power, CVA, [](float val) { return val / 100.0f; });
    this->update_sensor_from_s16_register16_(chan->power_factor, CPF,
                                             [](float val) { return std::abs(val / -327.68f); });
    this->update_sensor_from_s32_register16_(chan->forward_active_energy, CFWATTHR, [&chan](float val) {
      return chan->forward_active_energy_total += val / 14400.0f;
    });
    this->update_sensor_from_s32_register16_(chan->reverse_active_energy, CFWATTHR, [&chan](float val) {
      return chan->reverse_active_energy_total += val / 14400.0f;
    });
  }

  ESP_LOGD(TAG, "update took %" PRIu32 " ms", millis() - start);
}

void ADE7880::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7880:");
  LOG_PIN("  IRQ0  Pin: ", this->irq0_pin_);
  LOG_PIN("  IRQ1  Pin: ", this->irq1_pin_);
  LOG_PIN("  RESET Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.0f Hz", this->frequency_);

  if (this->channel_a_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Phase A:");
    LOG_SENSOR("    ", "Current", this->channel_a_->current);
    LOG_SENSOR("    ", "Voltage", this->channel_a_->voltage);
    LOG_SENSOR("    ", "Active Power", this->channel_a_->active_power);
    LOG_SENSOR("    ", "Apparent Power", this->channel_a_->apparent_power);
    LOG_SENSOR("    ", "Power Factor", this->channel_a_->power_factor);
    LOG_SENSOR("    ", "Forward Active Energy", this->channel_a_->forward_active_energy);
    LOG_SENSOR("    ", "Reverse Active Energy", this->channel_a_->reverse_active_energy);
    ESP_LOGCONFIG(TAG, "    Calibration:");
    ESP_LOGCONFIG(TAG, "     Current: %" PRId32, this->channel_a_->current_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Voltage: %" PRId32, this->channel_a_->voltage_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Power: %" PRId32, this->channel_a_->power_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Phase Angle: %u", this->channel_a_->phase_angle_calibration);
  }

  if (this->channel_b_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Phase B:");
    LOG_SENSOR("    ", "Current", this->channel_b_->current);
    LOG_SENSOR("    ", "Voltage", this->channel_b_->voltage);
    LOG_SENSOR("    ", "Active Power", this->channel_b_->active_power);
    LOG_SENSOR("    ", "Apparent Power", this->channel_b_->apparent_power);
    LOG_SENSOR("    ", "Power Factor", this->channel_b_->power_factor);
    LOG_SENSOR("    ", "Forward Active Energy", this->channel_b_->forward_active_energy);
    LOG_SENSOR("    ", "Reverse Active Energy", this->channel_b_->reverse_active_energy);
    ESP_LOGCONFIG(TAG, "    Calibration:");
    ESP_LOGCONFIG(TAG, "     Current: %" PRId32, this->channel_b_->current_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Voltage: %" PRId32, this->channel_b_->voltage_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Power: %" PRId32, this->channel_b_->power_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Phase Angle: %u", this->channel_b_->phase_angle_calibration);
  }

  if (this->channel_c_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Phase C:");
    LOG_SENSOR("    ", "Current", this->channel_c_->current);
    LOG_SENSOR("    ", "Voltage", this->channel_c_->voltage);
    LOG_SENSOR("    ", "Active Power", this->channel_c_->active_power);
    LOG_SENSOR("    ", "Apparent Power", this->channel_c_->apparent_power);
    LOG_SENSOR("    ", "Power Factor", this->channel_c_->power_factor);
    LOG_SENSOR("    ", "Forward Active Energy", this->channel_c_->forward_active_energy);
    LOG_SENSOR("    ", "Reverse Active Energy", this->channel_c_->reverse_active_energy);
    ESP_LOGCONFIG(TAG, "    Calibration:");
    ESP_LOGCONFIG(TAG, "     Current: %" PRId32, this->channel_c_->current_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Voltage: %" PRId32, this->channel_c_->voltage_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Power: %" PRId32, this->channel_c_->power_gain_calibration);
    ESP_LOGCONFIG(TAG, "     Phase Angle: %u", this->channel_c_->phase_angle_calibration);
  }

  if (this->channel_n_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Neutral:");
    LOG_SENSOR("    ", "Current", this->channel_n_->current);
    ESP_LOGCONFIG(TAG, "    Calibration:");
    ESP_LOGCONFIG(TAG, "     Current: %" PRId32, this->channel_n_->current_gain_calibration);
  }

  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

void ADE7880::calibrate_s10zp_reading_(uint16_t a_register, int16_t calibration) {
  if (calibration == 0) {
    return;
  }

  this->write_s10zp_register16_(a_register, calibration);
}

void ADE7880::calibrate_s24zpse_reading_(uint16_t a_register, int32_t calibration) {
  if (calibration == 0) {
    return;
  }

  this->write_s24zpse_register16_(a_register, calibration);
}

void ADE7880::init_device_() {
  this->write_u8_register16_(CONFIG2, CONFIG2_I2C_LOCK);

  this->write_u16_register16_(GAIN, 0);

  if (this->frequency_ > 55) {
    this->write_u16_register16_(COMPMODE, COMPMODE_DEFAULT | COMPMODE_SELFREQ);
  }

  if (this->channel_n_ != nullptr) {
    this->calibrate_s24zpse_reading_(NIGAIN, this->channel_n_->current_gain_calibration);
  }

  if (this->channel_a_ != nullptr) {
    this->calibrate_s24zpse_reading_(AIGAIN, this->channel_a_->current_gain_calibration);
    this->calibrate_s24zpse_reading_(AVGAIN, this->channel_a_->voltage_gain_calibration);
    this->calibrate_s24zpse_reading_(APGAIN, this->channel_a_->power_gain_calibration);
    this->calibrate_s10zp_reading_(APHCAL, this->channel_a_->phase_angle_calibration);
  }

  if (this->channel_b_ != nullptr) {
    this->calibrate_s24zpse_reading_(BIGAIN, this->channel_b_->current_gain_calibration);
    this->calibrate_s24zpse_reading_(BVGAIN, this->channel_b_->voltage_gain_calibration);
    this->calibrate_s24zpse_reading_(BPGAIN, this->channel_b_->power_gain_calibration);
    this->calibrate_s10zp_reading_(BPHCAL, this->channel_b_->phase_angle_calibration);
  }

  if (this->channel_c_ != nullptr) {
    this->calibrate_s24zpse_reading_(CIGAIN, this->channel_c_->current_gain_calibration);
    this->calibrate_s24zpse_reading_(CVGAIN, this->channel_c_->voltage_gain_calibration);
    this->calibrate_s24zpse_reading_(CPGAIN, this->channel_c_->power_gain_calibration);
    this->calibrate_s10zp_reading_(CPHCAL, this->channel_c_->phase_angle_calibration);
  }

  // write three default values to data memory RAM to flush the I2C write queue
  this->write_s32_register16_(VLEVEL, 0);
  this->write_s32_register16_(VLEVEL, 0);
  this->write_s32_register16_(VLEVEL, 0);

  this->write_u8_register16_(DSPWP_SEL, DSPWP_SEL_SET);
  this->write_u8_register16_(DSPWP_SET, DSPWP_SET_RO);
  this->write_u16_register16_(RUN, RUN_ENABLE);
}

void ADE7880::reset_device_() {
  if (this->reset_pin_ != nullptr) {
    ESP_LOGD(TAG, "Reset device using RESET pin");
    this->reset_pin_->digital_write(false);
    delay(1);
    this->reset_pin_->digital_write(true);
  } else {
    ESP_LOGD(TAG, "Reset device using SWRST command");
    this->write_u16_register16_(CONFIG, CONFIG_SWRST);
  }
  this->store_.reset_pending = true;
}

}  // namespace ade7880
}  // namespace esphome

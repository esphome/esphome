#include "cs5460a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cs5460a {

static const char *const TAG = "cs5460a";

void CS5460AComponent::write_register_(enum CS5460ARegister addr, uint32_t value) {
  this->write_byte(CMD_WRITE | (addr << 1));
  this->write_byte(value >> 16);
  this->write_byte(value >> 8);
  this->write_byte(value >> 0);
}

uint32_t CS5460AComponent::read_register_(uint8_t addr) {
  uint32_t value;

  this->write_byte(CMD_READ | (addr << 1));
  value = (uint32_t) this->transfer_byte(CMD_SYNC0) << 16;
  value |= (uint32_t) this->transfer_byte(CMD_SYNC0) << 8;
  value |= this->transfer_byte(CMD_SYNC0) << 0;

  return value;
}

bool CS5460AComponent::softreset_() {
  uint32_t pc = ((uint8_t) phase_offset_ & 0x3f) | (phase_offset_ < 0 ? 0x40 : 0);
  uint32_t config = (1 << 0) |                    /* K = 0b0001 */
                    (current_hpf_ ? 1 << 5 : 0) | /* IHPF */
                    (voltage_hpf_ ? 1 << 6 : 0) | /* VHPF */
                    (pga_gain_ << 16) |           /* Gi */
                    (pc << 17);                   /* PC */
  int cnt = 0;

  /* Serial resynchronization */
  this->write_byte(CMD_SYNC1);
  this->write_byte(CMD_SYNC1);
  this->write_byte(CMD_SYNC1);
  this->write_byte(CMD_SYNC0);

  /* Reset */
  this->write_register_(REG_CONFIG, 1 << 7);
  delay(10);
  while (cnt++ < 50 && (this->read_register_(REG_CONFIG) & 0x81) != 0x000001)
    ;
  if (cnt > 50)
    return false;

  this->write_register_(REG_CONFIG, config);
  return true;
}

void CS5460AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CS5460A...");

  float current_full_scale = (pga_gain_ == CS5460A_PGA_GAIN_10X) ? 0.25 : 0.10;
  float voltage_full_scale = 0.25;
  current_multiplier_ = current_full_scale / (fabsf(current_gain_) * 0x1000000);
  voltage_multiplier_ = voltage_full_scale / (voltage_gain_ * 0x1000000);

  /*
   * Calculate power from the Energy register because the Power register
   * stores instantaneous power which varies a lot in each AC cycle,
   * while the Energy value is accumulated over the "computation cycle"
   * which should be an integer number of AC cycles.
   */
  power_multiplier_ =
      (current_full_scale * voltage_full_scale * 4096) / (current_gain_ * voltage_gain_ * samples_ * 0x800000);

  pulse_freq_ =
      (current_full_scale * voltage_full_scale) / (fabsf(current_gain_) * voltage_gain_ * pulse_energy_wh_ * 3600);

  hw_init_();
}

void CS5460AComponent::hw_init_() {
  this->spi_setup();
  this->enable();

  if (!this->softreset_()) {
    this->disable();
    ESP_LOGE(TAG, "CS5460A reset failed!");
    this->mark_failed();
    return;
  }

  uint32_t status = this->read_register_(REG_STATUS);
  ESP_LOGCONFIG(TAG, "  Version: %x", (status >> 6) & 7);

  this->write_register_(REG_CYCLE_COUNT, samples_);
  this->write_register_(REG_PULSE_RATE, lroundf(pulse_freq_ * 32.0f));

  /* Use one of the power saving features (assuming external oscillator), reset other CONTROL bits,
   * sometimes softreset_() is not enough */
  this->write_register_(REG_CONTROL, 0x000004);

  this->restart_();
  this->disable();
  ESP_LOGCONFIG(TAG, "  Init ok");
}

/* Doesn't reset the register values etc., just restarts the "computation cycle" */
void CS5460AComponent::restart_() {
  this->enable();
  /* Stop running conversion, wake up if needed */
  this->write_byte(CMD_POWER_UP);
  /* Start continuous conversion */
  this->write_byte(CMD_START_CONT);
  this->disable();

  this->started_();
}

void CS5460AComponent::started_() {
  /*
   * Try to guess when the next batch of results is going to be ready and
   * schedule next STATUS check some time before that moment.  This assumes
   * two things:
   *   * a new "computation cycle" started just now.  If it started some
   *     time ago we may be a late next time, but hopefully less late in each
   *     iteration -- that's why we schedule the next check in some 0.8 of
   *     the time we actually expect the next reading ready.
   *   * MCLK rate is 4.096MHz and K == 1.  If there's a CS5460A module in
   *     use with a different clock this will need to be parametrised.
   */
  expect_data_ts_ = millis() + samples_ * 1024 / 4096;

  schedule_next_check_();
}

void CS5460AComponent::schedule_next_check_() {
  int32_t time_left = expect_data_ts_ - millis();

  /* First try at 0.8 of the actual expected time (if it's in the future) */
  if (time_left > 0)
    time_left -= time_left / 5;

  if (time_left > -500) {
    /* But not sooner than in 30ms from now */
    if (time_left < 30)
      time_left = 30;
  } else {
    /*
     * If the measurement is more than 0.5s overdue start worrying.  The
     * device may be stuck because of an overcurrent error or similar,
     * from now on just retry every 1s.  After 15s try a reset, if it
     * fails we give up and mark the component "failed".
     */
    if (time_left > -15000) {
      time_left = 1000;
      this->status_momentary_warning("warning", 1000);
    } else {
      ESP_LOGCONFIG(TAG, "Device officially stuck, resetting");
      this->cancel_timeout("status-check");
      this->hw_init_();
      return;
    }
  }

  this->set_timeout("status-check", time_left, [this]() {
    if (!this->check_status_())
      this->schedule_next_check_();
  });
}

bool CS5460AComponent::check_status_() {
  this->enable();
  uint32_t status = this->read_register_(REG_STATUS);

  if (!(status & 0xcbf83c)) {
    this->disable();
    return false;
  }

  uint32_t clear = 1 << 20;

  /* TODO: Report if IC=0 but only once as it can't be cleared */

  if (status & (1 << 2)) {
    clear |= 1 << 2;
    ESP_LOGE(TAG, "Low supply detected");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 3)) {
    clear |= 1 << 3;
    ESP_LOGE(TAG, "Modulator oscillation on current channel");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 4)) {
    clear |= 1 << 4;
    ESP_LOGE(TAG, "Modulator oscillation on voltage channel");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 5)) {
    clear |= 1 << 5;
    ESP_LOGE(TAG, "Watch-dog timeout");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 11)) {
    clear |= 1 << 11;
    ESP_LOGE(TAG, "EOUT Energy Accumulation Register out of range");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 12)) {
    clear |= 1 << 12;
    ESP_LOGE(TAG, "Energy out of range");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 13)) {
    clear |= 1 << 13;
    ESP_LOGE(TAG, "RMS voltage out of range");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 14)) {
    clear |= 1 << 14;
    ESP_LOGE(TAG, "RMS current out of range");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 15)) {
    clear |= 1 << 15;
    ESP_LOGE(TAG, "Power calculation out of range");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 16)) {
    clear |= 1 << 16;
    ESP_LOGE(TAG, "Voltage out of range");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 17)) {
    clear |= 1 << 17;
    ESP_LOGE(TAG, "Current out of range");
    this->status_momentary_warning("warning", 500);
  }

  if (status & (1 << 19)) {
    clear |= 1 << 19;
    ESP_LOGE(TAG, "Divide overflowed");
  }

  if (status & (1 << 22)) {
    bool dir = status & (1 << 21);
    if (current_gain_ < 0)
      dir = !dir;
    ESP_LOGI(TAG, "Energy counter %s pulse", dir ? "negative" : "positive");
    clear |= 1 << 22;
  }

  uint32_t raw_current = 0; /* Calm the validators */
  uint32_t raw_voltage = 0;
  uint32_t raw_energy = 0;

  if (status & (1 << 23)) {
    clear |= 1 << 23;

    if (current_sensor_ != nullptr)
      raw_current = this->read_register_(REG_IRMS);

    if (voltage_sensor_ != nullptr)
      raw_voltage = this->read_register_(REG_VRMS);
  }

  if (status & ((1 << 23) | (1 << 5))) {
    /* Read to clear the WDT bit */
    raw_energy = this->read_register_(REG_E);
  }

  this->write_register_(REG_STATUS, clear);
  this->disable();

  /*
   * Schedule the next STATUS check assuming that DRDY was asserted very
   * recently, then publish the new values.  Do this last for reentrancy in
   * case the publish triggers a restart() or for whatever reason needs to
   * cancel the timeout set in schedule_next_check_(), or needs to use SPI.
   * If the current or power values haven't changed one bit it may be that
   * the chip somehow forgot to update the registers -- seen happening very
   * rarely.  In that case don't publish them because the user may have
   * the input connected to a multiplexer and may have switched channels
   * since the previous reading and we'd be publishing the stale value for
   * the new channel.  If the value *was* updated it's very unlikely that
   * it wouldn't have changed, especially power/energy which are affected
   * by the noise on both the current and value channels (in case of energy,
   * accumulated over many conversion cycles.)
   */
  if (status & (1 << 23)) {
    this->started_();

    if (current_sensor_ != nullptr && raw_current != prev_raw_current_) {
      current_sensor_->publish_state(raw_current * current_multiplier_);
      prev_raw_current_ = raw_current;
    }

    if (voltage_sensor_ != nullptr)
      voltage_sensor_->publish_state(raw_voltage * voltage_multiplier_);

    if (power_sensor_ != nullptr && raw_energy != prev_raw_energy_) {
      int32_t raw = (int32_t)(raw_energy << 8) >> 8; /* Sign-extend */
      power_sensor_->publish_state(raw * power_multiplier_);
      prev_raw_energy_ = raw_energy;
    }

    return true;
  }

  return false;
}

void CS5460AComponent::dump_config() {
  uint32_t state = this->get_component_state();

  ESP_LOGCONFIG(TAG, "CS5460A:");
  ESP_LOGCONFIG(TAG, "  Init status: %s",
                state == COMPONENT_STATE_LOOP ? "OK" : (state == COMPONENT_STATE_FAILED ? "failed" : "other"));
  LOG_PIN("  CS Pin: ", cs_);
  ESP_LOGCONFIG(TAG, "  Samples / cycle: %u", samples_);
  ESP_LOGCONFIG(TAG, "  Phase offset: %i", phase_offset_);
  ESP_LOGCONFIG(TAG, "  PGA Gain: %s", pga_gain_ == CS5460A_PGA_GAIN_50X ? "50x" : "10x");
  ESP_LOGCONFIG(TAG, "  Current gain: %.5f", current_gain_);
  ESP_LOGCONFIG(TAG, "  Voltage gain: %.5f", voltage_gain_);
  ESP_LOGCONFIG(TAG, "  Current HPF: %s", current_hpf_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Voltage HPF: %s", voltage_hpf_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Pulse energy: %.2f Wh", pulse_energy_wh_);
  LOG_SENSOR("  ", "Voltage", voltage_sensor_);
  LOG_SENSOR("  ", "Current", current_sensor_);
  LOG_SENSOR("  ", "Power", power_sensor_);
}

}  // namespace cs5460a
}  // namespace esphome

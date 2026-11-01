#include "lis3dh.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::lis3dh {

ESPHOME_LOG_TAG(TAG, "lis3dh");

#ifdef USE_LIS3DH_TAP
// Unlatched CLICK_SRC bits stay set for as little as 15 ms, so poll faster than the 16 ms loop
static constexpr uint32_t TAP_POLL_INTERVAL_MS = 10;

// Click threshold LSB in mg, indexed by range
static const uint16_t THRESHOLD_LSB_MG[] PROGMEM = {16, 32, 62, 186};
#endif

// g per LSB of the left-justified 16-bit sample, indexed by range. The same in every
// operating mode, since the chip leaves the unused low bits at zero.
static const float SCALE_G[] PROGMEM = {0.0625e-3f, 0.125e-3f, 0.25e-3f, 0.75e-3f};

static const LogString *mode_to_str(LIS3DHOperatingMode mode) {
  switch (mode) {
    case LIS3DH_MODE_LOW_POWER:
      return LOG_STR("low power (8-bit)");
    case LIS3DH_MODE_NORMAL:
      return LOG_STR("normal (10-bit)");
    default:
      return LOG_STR("high resolution (12-bit)");
  }
}

// Output data rate in Hz, indexed by the ODR field minus one.
static const uint16_t RATE_HZ[] PROGMEM = {1, 10, 25, 50, 100, 200, 400, 1620, 1344};

void LIS3DHComponent::setup() {
  MotionComponent::setup();

  if (!this->check_chip_id(this, LIS3DH_REG_WHO_AM_I, LIS3DH_WHO_AM_I_VALUE))
    return;

  if (!this->setup_accelerometer_() || !this->setup_temperature_() || !this->setup_interrupt_() ||
      !this->setup_tap_()) {
    this->mark_failed(LOG_STR("Failed to configure sensor"));
    return;
  }
#ifdef USE_LIS3DH_TAP
  // loop() only watches the tap interrupt pin; without one, CLICK_SRC is polled
  if (this->tap_axes_cfg_ == 0 || this->tap_interrupt_pin_ == nullptr) {
    this->disable_loop();
    if (this->tap_axes_cfg_ != 0) {
      this->set_interval(TAP_POLL_INTERVAL_MS, [this]() {
        const uint32_t now = millis();
        this->check_pending_tap_(now);
        this->read_tap_(now);
      });
    }
  }
#endif
}

#ifdef USE_LIS3DH_TAP
void LIS3DHComponent::loop() {
  // Only one trigger is allowed with an interrupt pin, so no single tap is ever held here.
  // The pad polarity is the chip-wide one set by the motion interrupt's active_high.
  if (this->tap_interrupt_pin_->digital_read() != this->interrupt_active_high_) {
    this->last_tap_src_ = 0;
    return;
  }
  this->read_tap_(App.get_loop_component_start_time());
}

void LIS3DHComponent::check_pending_tap_(uint32_t now) {
  if (this->tap_pending_ && now - this->tap_pending_since_ >= this->tap_double_wait_ms_ + TAP_POLL_INTERVAL_MS) {
    this->tap_pending_ = false;
    this->tap_callback_.call(this->tap_pending_axis_, this->tap_pending_negative_);
  }
}
#endif

bool LIS3DHComponent::setup_accelerometer_() {
  // CTRL_REG1: output data rate, low-power flag and axis enable.
  uint8_t ctrl_reg1 = ((uint8_t) this->data_rate_ << 4) | LIS3DH_CTRL_REG1_AXES_EN;
  if (this->operating_mode_ == LIS3DH_MODE_LOW_POWER)
    ctrl_reg1 |= LIS3DH_CTRL_REG1_LPEN;
  if (!this->write_byte(LIS3DH_REG_CTRL_REG1, ctrl_reg1))
    return false;

  // CTRL_REG4: block data update, full-scale range and high-resolution flag.
  uint8_t ctrl_reg4 = LIS3DH_CTRL_REG4_BDU | ((uint8_t) this->range_ << 4);
  if (this->operating_mode_ == LIS3DH_MODE_HIGH_RESOLUTION)
    ctrl_reg4 |= LIS3DH_CTRL_REG4_HR;
  return this->write_byte(LIS3DH_REG_CTRL_REG4, ctrl_reg4);
}

bool LIS3DHComponent::setup_temperature_() {
  // Only power the ADC / temperature sensor when a temperature sensor is used.
  uint8_t temp_cfg =
      this->temperature_callback_.empty() ? 0x00 : (uint8_t) (LIS3DH_TEMP_CFG_ADC_EN | LIS3DH_TEMP_CFG_TEMP_EN);
  return this->write_byte(LIS3DH_REG_TEMP_CFG, temp_cfg);
}

bool LIS3DHComponent::setup_interrupt_() {
  // Runs even with no interrupt configured: the chip usually stays powered across resets,
  // so this drops whatever the previous firmware left in the interrupt registers.
  // High-pass filter the interrupt (IA1) input so the ~1 g of gravity is removed
  // from the threshold comparison. The output data registers stay unfiltered
  // (FDS = 0), so acceleration / pitch / roll still report the full gravity vector.
  uint8_t ctrl_reg2 = this->interrupt_high_pass_ ? LIS3DH_CTRL_REG2_HP_IA1 : 0x00;
#ifdef USE_LIS3DH_TAP
  if (this->tap_axes_cfg_ != 0 && this->tap_high_pass_)
    ctrl_reg2 |= LIS3DH_CTRL_REG2_HPCLICK;
#endif
  if (!this->write_byte(LIS3DH_REG_CTRL_REG2, ctrl_reg2))
    return false;

  if (!this->write_byte(LIS3DH_REG_INT1_CFG, this->interrupt_axes_cfg_) ||
      !this->write_byte(LIS3DH_REG_INT1_THS, this->interrupt_threshold_) ||
      !this->write_byte(LIS3DH_REG_INT1_DURATION, this->interrupt_duration_))
    return false;

  // Latch the request so a brief movement keeps the pad asserted until INT1_SRC is read.
  uint8_t ctrl_reg5 = this->interrupt_latched_ ? LIS3DH_CTRL_REG5_LIR_INT1 : 0;
  if (!this->write_byte(LIS3DH_REG_CTRL_REG5, ctrl_reg5))
    return false;

  // Route the IA1 engine to the chosen physical pad and set the pad polarity.
  uint8_t ctrl_reg3 = 0;
  uint8_t ctrl_reg6 = this->interrupt_active_high_ ? 0 : LIS3DH_CTRL_REG6_INT_POLARITY;
  if (this->interrupt_axes_cfg_ != 0) {
    if (this->interrupt_pin_ == LIS3DH_INT_PIN_INT1) {
      ctrl_reg3 |= LIS3DH_CTRL_REG3_I1_IA1;
    } else {
      ctrl_reg6 |= LIS3DH_CTRL_REG6_I2_IA1;
    }
  }
#ifdef USE_LIS3DH_TAP
  // Route clicks to a pad only when a pin watches it
  if (this->tap_axes_cfg_ != 0 && this->tap_interrupt_pin_ != nullptr) {
    if (this->tap_pad_ == LIS3DH_INT_PIN_INT1) {
      ctrl_reg3 |= LIS3DH_CTRL_REG3_I1_CLICK;
    } else {
      ctrl_reg6 |= LIS3DH_CTRL_REG6_I2_CLICK;
    }
  }
#endif
  if (!this->write_byte(LIS3DH_REG_CTRL_REG3, ctrl_reg3) || !this->write_byte(LIS3DH_REG_CTRL_REG6, ctrl_reg6))
    return false;
  if (this->interrupt_axes_cfg_ == 0)
    return true;

  // With the high-pass filter enabled, reading REFERENCE captures the current
  // acceleration as the filter's zero point (removing the standing gravity bias).
  if (this->interrupt_high_pass_) {
    uint8_t reference = 0;
    this->read_byte(LIS3DH_REG_REFERENCE, &reference);
  }

  // Clear any latched request left over from the motion that woke the device.
  uint8_t src = 0;
  this->read_byte(LIS3DH_REG_INT1_SRC, &src);
  return true;
}

bool LIS3DHComponent::setup_tap_() {
#ifdef USE_LIS3DH_TAP
  if (this->tap_axes_cfg_ != 0) {
    if (this->tap_interrupt_pin_ != nullptr)
      this->tap_interrupt_pin_->setup();
    // CLICK_THS through TIME_WINDOW in one burst. LIR_Click (CLICK_THS bit 7) stays clear: latching
    // makes a polled CLICK_SRC read reset the double-tap state machine.
    const uint8_t timing[] = {this->tap_threshold_, this->tap_time_limit_, this->tap_latency_, this->tap_window_};
    // Axes last, so the detector never runs with half-written settings
    if (!this->write_bytes(LIS3DH_REG_CLICK_THS | LIS3DH_AUTO_INCREMENT, timing, sizeof(timing)) ||
        !this->write_byte(LIS3DH_REG_CLICK_CFG, this->tap_axes_cfg_))
      return false;
    // Clear anything detected while the settings were written
    uint8_t src = 0;
    this->read_byte(LIS3DH_REG_CLICK_SRC, &src);
    return true;
  }
#endif
  // Also runs with tap detection off, to clear a configuration left by earlier firmware
  return this->write_byte(LIS3DH_REG_CLICK_CFG, 0x00);
}

#ifdef USE_LIS3DH_TAP
void LIS3DHComponent::set_tap_threshold(float threshold_g) {
  const uint16_t lsb_mg = progmem_read_uint16(&THRESHOLD_LSB_MG[this->range_]);
  const int32_t steps = static_cast<int32_t>(std::lround(threshold_g * 1000.0f / lsb_mg));
  this->tap_threshold_ = (uint8_t) std::clamp<int32_t>(steps, 1, LIS3DH_CLICK_THS_MAX);
  if (this->tap_axes_cfg_ == 0 || !this->is_ready())
    return;
  if (!this->write_byte(LIS3DH_REG_CLICK_THS, this->tap_threshold_)) {
    this->status_set_warning(LOG_STR("Failed to set tap threshold"));
    return;
  }
  ESP_LOGD(TAG, "Tap threshold set to %u steps", this->tap_threshold_);
}

void LIS3DHComponent::read_tap_(uint32_t now) {
  uint8_t src;
  if (!this->read_byte(LIS3DH_REG_CLICK_SRC, &src)) {
    this->status_set_warning(LOG_STR("Failed to read tap status"));
    return;
  }
  // CLICK_SRC is not latched, so act only on bits that just rose
  const uint8_t rising = src & ~this->last_tap_src_;
  this->last_tap_src_ = src;
  if (rising == 0)
    return;
  ESP_LOGV(TAG, "CLICK_SRC: 0x%02X", src);

  uint8_t axis;
  bool negative;
  this->resolve_tap_axis_(src, axis, negative);
  if ((this->tap_axes_cfg_ & LIS3DH_CLICK_CFG_SINGLE) && (this->tap_axes_cfg_ & LIS3DH_CLICK_CFG_DOUBLE)) {
    // Both enabled: IA means a double tap, and a lone SCLICK is held until the window passes
    if ((rising & LIS3DH_CLICK_SRC_IA) && (src & LIS3DH_CLICK_SRC_DCLICK)) {
      this->tap_pending_ = false;
      this->double_tap_callback_.call(axis, negative);
    } else if ((rising & LIS3DH_CLICK_SRC_SCLICK) && !this->tap_pending_) {
      this->tap_pending_axis_ = axis;
      this->tap_pending_negative_ = negative;
      this->tap_pending_since_ = now;
      this->tap_pending_ = true;
    }
    return;
  }

  if ((rising & LIS3DH_CLICK_SRC_IA) == 0)
    return;
  if (src & LIS3DH_CLICK_SRC_SCLICK)
    this->tap_callback_.call(axis, negative);
  if (src & LIS3DH_CLICK_SRC_DCLICK)
    this->double_tap_callback_.call(axis, negative);
}

void LIS3DHComponent::resolve_tap_axis_(uint8_t src, uint8_t &axis, bool &negative) const {
  // In the axis_map frame; the base matrix, so level calibration does not move it
  const uint8_t chip_axis = (src & LIS3DH_CLICK_SRC_X) ? 0 : (src & LIS3DH_CLICK_SRC_Y) ? 1 : 2;
  axis = 0;
  float weight = 0.0f;
  for (uint8_t i = 0; i < 3; i++) {
    const float w = this->base_matrix_[i * 3 + chip_axis];
    if (std::fabs(w) > std::fabs(weight)) {
      weight = w;
      axis = i;
    }
  }
  negative = ((src & LIS3DH_CLICK_SRC_SIGN) != 0) != (weight < 0.0f);
}
#endif

void LIS3DHComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LIS3DH:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
    return;

  ESP_LOGCONFIG(TAG,
                "  Range: ±%ug\n"
                "  Operating mode: %s\n"
                "  Data rate: %uHz",
                2u << this->range_, LOG_STR_ARG(mode_to_str(this->operating_mode_)),
                (unsigned) progmem_read_uint16(&RATE_HZ[this->data_rate_ - 1]));
  if (this->interrupt_axes_cfg_ != 0) {
    ESP_LOGCONFIG(TAG,
                  "  Motion interrupt: %s (%s%s%s)\n"
                  "    Threshold: %u steps\n"
                  "    Duration: %u samples",
                  this->interrupt_pin_ == LIS3DH_INT_PIN_INT1 ? LOG_STR_LITERAL("INT1") : LOG_STR_LITERAL("INT2"),
                  this->interrupt_active_high_ ? LOG_STR_LITERAL("active high") : LOG_STR_LITERAL("active low"),
                  this->interrupt_high_pass_ ? LOG_STR_LITERAL(", high-pass") : LOG_STR_LITERAL(""),
                  this->interrupt_latched_ ? LOG_STR_LITERAL(", latched") : LOG_STR_LITERAL(""),
                  this->interrupt_threshold_, this->interrupt_duration_);
  }
#ifdef USE_LIS3DH_TAP
  if (this->tap_axes_cfg_ != 0) {
    ESP_LOGCONFIG(TAG,
                  "  Tap detection: %s\n"
                  "    Threshold: %u steps\n"
                  "    Time limit: %u, latency: %u, window: %u samples",
                  this->tap_interrupt_pin_ == nullptr     ? LOG_STR_LITERAL("polled")
                  : this->tap_pad_ == LIS3DH_INT_PIN_INT1 ? LOG_STR_LITERAL("INT1")
                                                          : LOG_STR_LITERAL("INT2"),
                  this->tap_threshold_, this->tap_time_limit_, this->tap_latency_, this->tap_window_);
    LOG_PIN("    Interrupt pin: ", this->tap_interrupt_pin_);
  }
#endif
  MotionComponent::dump_config();
}

void LIS3DHComponent::on_shutdown() {
  // Clear a latched request from motion seen while awake, so it does not wake the device straight away.
  if (this->interrupt_latched_ && this->interrupt_axes_cfg_ != 0 && !this->is_failed()) {
    uint8_t src = 0;
    this->read_byte(LIS3DH_REG_INT1_SRC, &src);
  }
}

bool LIS3DHComponent::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  int16_t raw[3];
  if (!this->read_bytes_16_le(LIS3DH_REG_OUT_X_L | LIS3DH_AUTO_INCREMENT, raw, 3)) {
    this->status_set_warning(LOG_STR("Failed to read acceleration data"));
    return false;
  }

  float scale;
  progmem_memcpy(&scale, &SCALE_G[this->range_], sizeof(scale));

  // Data is little-endian and left-justified in a signed 16-bit register.
  int16_t raw_x = raw[0];
  int16_t raw_y = raw[1];
  int16_t raw_z = raw[2];
  ESP_LOGV(TAG, "Read raw accel data: %d, %d, %d", raw_x, raw_y, raw_z);
  data.acceleration[motion::X_AXIS] = raw_x * scale;
  data.acceleration[motion::Y_AXIS] = raw_y * scale;
  data.acceleration[motion::Z_AXIS] = raw_z * scale;

  if (this->temperature_callback_.empty() || this->publish_temperature_())
    this->status_clear_warning();
  return true;
}

bool LIS3DHComponent::publish_temperature_() {
  int16_t raw;
  if (!this->read_bytes_16_le(LIS3DH_REG_OUT_ADC3_L | LIS3DH_AUTO_INCREMENT, &raw, 1)) {
    this->status_set_warning(LOG_STR("Failed to read temperature data"));
    return false;
  }
  // Signed, left-justified, relative value with 1 °C per count of the high byte; the
  // low byte carries the fractional bits in the modes that have them.
  float temperature = raw / 256.0f + LIS3DH_TEMP_REFERENCE_C;
  this->temperature_callback_.call(temperature);
  return true;
}

}  // namespace esphome::lis3dh

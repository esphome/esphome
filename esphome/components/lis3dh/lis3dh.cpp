#include "lis3dh.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace lis3dh {

static const char *const TAG = "lis3dh";

namespace {

int32_t lis3dh_write_bridge(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len) {
  auto *self = static_cast<LIS3DHComponent *>(handle);
  return self->write_register(reg, bufp, len) ? 0 : -1;
}

int32_t lis3dh_read_bridge(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len) {
  auto *self = static_cast<LIS3DHComponent *>(handle);
  return self->read_register(reg, bufp, len) ? 0 : -1;
}

const char *range_to_str(LIS3DHRange r) {
  switch (r) {
    case LIS3DHRange::RANGE_2G:
      return "2G";
    case LIS3DHRange::RANGE_4G:
      return "4G";
    case LIS3DHRange::RANGE_8G:
      return "8G";
    case LIS3DHRange::RANGE_16G:
      return "16G";
  }
  return "unknown";
}

const char *mode_to_str(LIS3DHMode m) {
  switch (m) {
    case LIS3DHMode::MODE_LOW_POWER:
      return "Low Power";
    case LIS3DHMode::MODE_NORMAL:
      return "Normal";
    case LIS3DHMode::MODE_HIGH_RESOLUTION:
      return "High Resolution";
  }
  return "unknown";
}

const char *odr_to_str(LIS3DHDataRate r) {
  switch (r) {
    case LIS3DHDataRate::ODR_1HZ:
      return "1 Hz";
    case LIS3DHDataRate::ODR_10HZ:
      return "10 Hz";
    case LIS3DHDataRate::ODR_25HZ:
      return "25 Hz";
    case LIS3DHDataRate::ODR_50HZ:
      return "50 Hz";
    case LIS3DHDataRate::ODR_100HZ:
      return "100 Hz";
    case LIS3DHDataRate::ODR_200HZ:
      return "200 Hz";
    case LIS3DHDataRate::ODR_400HZ:
      return "400 Hz";
    case LIS3DHDataRate::ODR_1600HZ:
      return "1.6 kHz";
    case LIS3DHDataRate::ODR_5376HZ:
      return "5.376 kHz";
  }
  return "unknown";
}

const char *orientation_xy_to_str(LIS3DHOrientationXY o) {
  switch (o) {
    case LIS3DHOrientationXY::PORTRAIT_UPRIGHT:
      return "Portrait Upright";
    case LIS3DHOrientationXY::PORTRAIT_UPSIDE_DOWN:
      return "Portrait Upside Down";
    case LIS3DHOrientationXY::LANDSCAPE_LEFT:
      return "Landscape Left";
    case LIS3DHOrientationXY::LANDSCAPE_RIGHT:
      return "Landscape Right";
    case LIS3DHOrientationXY::FLAT:
      return "Flat";
  }
  return "unknown";
}

const char *orientation_z_to_str(LIS3DHOrientationZ o) {
  return o == LIS3DHOrientationZ::FACE_UP ? "Face Up" : "Face Down";
}

}  // namespace

void IRAM_ATTR LIS3DHStore::int1_gpio_intr(LIS3DHStore *arg) {
  arg->int1_triggered = true;
  if (arg->parent != nullptr)
    arg->parent->enable_loop_soon_any_context();
}

void IRAM_ATTR LIS3DHStore::int2_gpio_intr(LIS3DHStore *arg) {
  arg->int2_triggered = true;
  if (arg->parent != nullptr)
    arg->parent->enable_loop_soon_any_context();
}

void LIS3DHComponent::setup() {
  this->dev_ctx_.handle = this;
  this->dev_ctx_.write_reg = lis3dh_write_bridge;
  this->dev_ctx_.read_reg = lis3dh_read_bridge;
  this->dev_ctx_.mdelay = [](uint32_t ms) { delay_microseconds_safe(ms * 1000); };

  if (!this->verify_device_id_()) {
    this->mark_failed();
    return;
  }
  if (!this->configure_device_()) {
    this->mark_failed();
    return;
  }

  if (this->fifo_enabled_ && !this->configure_fifo_()) {
    ESP_LOGW(TAG, "Failed to configure FIFO");
  }
  if (this->tap_enabled_ && !this->configure_tap_detection_()) {
    ESP_LOGW(TAG, "Failed to configure tap detection");
  }
  if (this->activity_enabled_ && !this->configure_motion_detection_()) {
    ESP_LOGW(TAG, "Failed to configure motion detection");
  }
  if (this->freefall_enabled_ && !this->configure_freefall_detection_()) {
    ESP_LOGW(TAG, "Failed to configure free-fall detection");
  }
  if (this->auto_low_power_enabled_) {
    if (!this->configure_auto_low_power_()) {
      ESP_LOGW(TAG, "Failed to configure auto low-power mode");
    }
  } else if (lis3dh_act_threshold_set(&this->dev_ctx_, 0) != 0) {
    // ACT_THS resets to 0 (disabled) on power-on, but the chip keeps its registers across
    // ESP32 deep sleep/reboots, so an explicit disable is needed if the config changes.
    ESP_LOGW(TAG, "Failed to disable auto low-power mode");
  }
  if (!this->configure_interrupt_routing_()) {
    ESP_LOGW(TAG, "Failed to configure interrupt routing");
  }

  if (this->enable_deep_sleep_wakeup_) {
    // A latched interrupt from before the last deep sleep can leave INT1 stuck high, which
    // would make the ESP32 deep_sleep component see the wakeup pin as already asserted and
    // refuse to sleep (or wake immediately). Read out the latch sources once at boot so the
    // pin starts low.
    uint8_t dummy;
    this->read_register(LIS3DH_INT1_SRC, &dummy, 1);
    this->read_register(LIS3DH_CLICK_SRC, &dummy, 1);
  }

  this->store_.parent = this;

  if (this->int1_pin_ != nullptr) {
    this->int1_pin_->setup();
    this->store_.int1_pin = this->int1_pin_->to_isr();
    this->int1_pin_->attach_interrupt(LIS3DHStore::int1_gpio_intr, &this->store_, gpio::INTERRUPT_RISING_EDGE);
  }
  if (this->int2_pin_ != nullptr) {
    this->int2_pin_->setup();
    this->store_.int2_pin = this->int2_pin_->to_isr();
    this->int2_pin_->attach_interrupt(LIS3DHStore::int2_gpio_intr, &this->store_, gpio::INTERRUPT_RISING_EDGE);
  }

  // loop() only runs when an ISR re-enables it. If no INT pins are wired,
  // event sources are polled in update() instead.
  this->disable_loop();
}

void LIS3DHComponent::update() {
  this->read_acceleration_data_();
#ifdef USE_SENSOR
  if (this->temperature_sensor_ != nullptr) {
    this->read_temperature_data_();
  }
#endif

  this->update_orientation_();

  if (this->fifo_enabled_) {
    this->read_fifo_data_();
  }

  // Polling fallback: if no INT pins are wired, drain event sources here.
  if (!this->interrupt_driven_()) {
    this->process_event_sources_();
  }
}

void LIS3DHComponent::loop() {
  if (this->store_.int1_triggered || this->store_.int2_triggered) {
    this->store_.int1_triggered = false;
    this->store_.int2_triggered = false;
    this->process_event_sources_();
  }
  // Park the loop until the next ISR wakes us.
  this->disable_loop();
}

void LIS3DHComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LIS3DH:");
  ESP_LOGCONFIG(TAG, "  Range: %s", range_to_str(this->range_));
  ESP_LOGCONFIG(TAG, "  Mode: %s", mode_to_str(this->mode_));
  ESP_LOGCONFIG(TAG, "  Data rate: %s", odr_to_str(this->data_rate_));
  ESP_LOGCONFIG(TAG, "  FIFO: %s", YESNO(this->fifo_enabled_));
  if (this->fifo_enabled_) {
    ESP_LOGCONFIG(TAG, "  FIFO watermark: %u", this->fifo_watermark_);
  }
  ESP_LOGCONFIG(TAG, "  Tap detection: %s", YESNO(this->tap_enabled_));
  ESP_LOGCONFIG(TAG, "  Activity detection: %s", YESNO(this->activity_enabled_));
  ESP_LOGCONFIG(TAG, "  Free-fall detection: %s", YESNO(this->freefall_enabled_));
  LOG_PIN("  INT1 pin: ", this->int1_pin_);
  LOG_PIN("  INT2 pin: ", this->int2_pin_);
  ESP_LOGCONFIG(TAG, "  Event source: %s", this->interrupt_driven_() ? "interrupt" : "polled (no INT pin wired)");
  ESP_LOGCONFIG(TAG, "  Deep sleep wakeup: %s", YESNO(this->enable_deep_sleep_wakeup_));
  ESP_LOGCONFIG(TAG, "  Auto low-power mode (10 Hz when still): %s", YESNO(this->auto_low_power_enabled_));
  ESP_LOGCONFIG(TAG, "  Temperature: %s", YESNO(this->temperature_enabled_));
  LOG_UPDATE_INTERVAL(this);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Acceleration X", this->acceleration_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->acceleration_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->acceleration_z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Tap", this->tap_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Double Tap", this->double_tap_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Activity", this->activity_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Free-fall", this->freefall_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Orientation XY", this->orientation_xy_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Orientation Z", this->orientation_z_text_sensor_);
#endif
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with LIS3DH failed");
  }
}

bool LIS3DHComponent::write_register_verified_(uint8_t reg, uint8_t value) {
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!this->write_register(reg, &value, 1))
      return false;
    uint8_t readback = 0;
    if (!this->read_register(reg, &readback, 1))
      return false;
    if (readback == value)
      return true;
  }
  return false;
}

bool LIS3DHComponent::verify_device_id_() {
  uint8_t whoami = 0;
  if (!this->read_register(LIS3DH_WHO_AM_I, &whoami, 1)) {
    ESP_LOGE(TAG, "Failed to read WHO_AM_I register");
    return false;
  }
  if (whoami != LIS3DH_ID) {
    ESP_LOGE(TAG, "Invalid device ID: 0x%02X (expected 0x%02X)", whoami, LIS3DH_ID);
    return false;
  }
  return true;
}

bool LIS3DHComponent::configure_device_() {
  if (lis3dh_block_data_update_set(&this->dev_ctx_, PROPERTY_ENABLE) != 0) {
    return false;
  }

  // Explicitly enable all three axes. No ST driver helper touches CTRL_REG1's Xen/Yen/Zen bits
  // (the ODR/mode setters only modify their own fields), so if a previous session left them
  // cleared -- the LIS3DH keeps its registers across ESP32 resets/deep sleep -- the output
  // registers would otherwise stay frozen at their last converted value forever.
  lis3dh_ctrl_reg1_t ctrl_reg1{};
  if (!this->read_register(LIS3DH_CTRL_REG1, reinterpret_cast<uint8_t *>(&ctrl_reg1), 1)) {
    return false;
  }
  ctrl_reg1.xen = 1;
  ctrl_reg1.yen = 1;
  ctrl_reg1.zen = 1;
  if (!this->write_register(LIS3DH_CTRL_REG1, reinterpret_cast<uint8_t *>(&ctrl_reg1), 1)) {
    return false;
  }

  if (this->temperature_enabled_ && lis3dh_aux_adc_set(&this->dev_ctx_, LIS3DH_AUX_ON_TEMPERATURE) != 0) {
    return false;
  }

  lis3dh_fs_t fs;
  switch (this->range_) {
    case LIS3DHRange::RANGE_2G:
      fs = LIS3DH_2g;
      break;
    case LIS3DHRange::RANGE_4G:
      fs = LIS3DH_4g;
      break;
    case LIS3DHRange::RANGE_8G:
      fs = LIS3DH_8g;
      break;
    case LIS3DHRange::RANGE_16G:
      fs = LIS3DH_16g;
      break;
  }
  if (lis3dh_full_scale_set(&this->dev_ctx_, fs) != 0) {
    return false;
  }

  lis3dh_op_md_t op_md;
  switch (this->mode_) {
    case LIS3DHMode::MODE_LOW_POWER:
      op_md = LIS3DH_LP_8bit;
      break;
    case LIS3DHMode::MODE_NORMAL:
      op_md = LIS3DH_NM_10bit;
      break;
    case LIS3DHMode::MODE_HIGH_RESOLUTION:
      op_md = LIS3DH_HR_12bit;
      break;
  }
  if (lis3dh_operating_mode_set(&this->dev_ctx_, op_md) != 0) {
    return false;
  }

  lis3dh_odr_t odr;
  switch (this->data_rate_) {
    case LIS3DHDataRate::ODR_1HZ:
      odr = LIS3DH_ODR_1Hz;
      break;
    case LIS3DHDataRate::ODR_10HZ:
      odr = LIS3DH_ODR_10Hz;
      break;
    case LIS3DHDataRate::ODR_25HZ:
      odr = LIS3DH_ODR_25Hz;
      break;
    case LIS3DHDataRate::ODR_50HZ:
      odr = LIS3DH_ODR_50Hz;
      break;
    case LIS3DHDataRate::ODR_100HZ:
      odr = LIS3DH_ODR_100Hz;
      break;
    case LIS3DHDataRate::ODR_200HZ:
      odr = LIS3DH_ODR_200Hz;
      break;
    case LIS3DHDataRate::ODR_400HZ:
      odr = LIS3DH_ODR_400Hz;
      break;
    case LIS3DHDataRate::ODR_1600HZ:
      odr = LIS3DH_ODR_1kHz620_LP;
      break;
    case LIS3DHDataRate::ODR_5376HZ:
      odr = LIS3DH_ODR_5kHz376_LP_1kHz344_NM_HP;
      break;
  }
  return lis3dh_data_rate_set(&this->dev_ctx_, odr) == 0;
}

bool LIS3DHComponent::configure_interrupt_routing_() {
  // Always wire chip-side routing so that INT1/INT2 pads carry the right
  // events. The MCU-side ISR is only attached if the user wired up the pin,
  // but the chip-level routing is harmless when the pad is unused.
  lis3dh_ctrl_reg3_t reg3{};
  reg3.i1_click = this->tap_enabled_ ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  reg3.i1_ia1 = this->activity_enabled_ ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  // FIFO is always drained unconditionally in update() regardless of the watermark interrupt
  // (see read_fifo_data_()), so nothing consumes this signal -- and routing it onto INT1 would
  // pin that pad high (blocking deep-sleep wakeup) any time the bus can't drain the FIFO faster
  // than ODR fills it. Never route it.
  reg3.i1_wtm = PROPERTY_DISABLE;
  if (lis3dh_pin_int1_config_set(&this->dev_ctx_, &reg3) != 0) {
    return false;
  }

  lis3dh_ctrl_reg6_t reg6{};
  reg6.i2_ia2 = this->freefall_enabled_ ? PROPERTY_ENABLE : PROPERTY_DISABLE;
  if (lis3dh_pin_int2_config_set(&this->dev_ctx_, &reg6) != 0) {
    return false;
  }

  // Latch INT1 whenever activity detection drives it, or whenever the caller wants INT1 to
  // hold the ESP32's deep-sleep wakeup GPIO asserted until the event is read out. INT2
  // (free-fall) is left non-latched so it tracks state.
  const bool latch_int1 = this->activity_enabled_ || this->enable_deep_sleep_wakeup_;
  if (latch_int1 && lis3dh_int1_pin_notification_mode_set(&this->dev_ctx_, LIS3DH_INT1_LATCHED) != 0) {
    return false;
  }
  return true;
}

bool LIS3DHComponent::configure_fifo_() {
  lis3dh_fm_t fifo_mode;
  switch (this->fifo_mode_) {
    case LIS3DHFifoMode::FIFO_BYPASS:
      fifo_mode = LIS3DH_BYPASS_MODE;
      break;
    case LIS3DHFifoMode::FIFO_MODE:
      fifo_mode = LIS3DH_FIFO_MODE;
      break;
    case LIS3DHFifoMode::FIFO_STREAM:
      fifo_mode = LIS3DH_DYNAMIC_STREAM_MODE;
      break;
    case LIS3DHFifoMode::FIFO_STREAM_TO_FIFO:
      fifo_mode = LIS3DH_STREAM_TO_FIFO_MODE;
      break;
  }
  if (lis3dh_fifo_mode_set(&this->dev_ctx_, fifo_mode) != 0)
    return false;
  if (lis3dh_fifo_watermark_set(&this->dev_ctx_, this->fifo_watermark_) != 0)
    return false;
  return lis3dh_fifo_set(&this->dev_ctx_, PROPERTY_ENABLE) == 0;
}

bool LIS3DHComponent::configure_tap_detection_() {
  lis3dh_click_cfg_t click_cfg{};
  click_cfg.xs = PROPERTY_ENABLE;
  click_cfg.ys = PROPERTY_ENABLE;
  click_cfg.zs = PROPERTY_ENABLE;
  click_cfg.xd = PROPERTY_ENABLE;
  click_cfg.yd = PROPERTY_ENABLE;
  click_cfg.zd = PROPERTY_ENABLE;

  if (lis3dh_tap_conf_set(&this->dev_ctx_, &click_cfg) != 0)
    return false;
  if (lis3dh_tap_threshold_set(&this->dev_ctx_, this->tap_threshold_) != 0)
    return false;
  if (lis3dh_shock_dur_set(&this->dev_ctx_, this->tap_shock_duration_) != 0)
    return false;
  if (lis3dh_quiet_dur_set(&this->dev_ctx_, this->tap_quiet_duration_) != 0)
    return false;
  return lis3dh_double_tap_timeout_set(&this->dev_ctx_, this->tap_double_tap_timeout_) == 0;
}

bool LIS3DHComponent::configure_motion_detection_() {
  lis3dh_int1_cfg_t int1_cfg{};
  int1_cfg.xlie = PROPERTY_ENABLE;
  int1_cfg.xhie = PROPERTY_ENABLE;
  int1_cfg.ylie = PROPERTY_ENABLE;
  int1_cfg.yhie = PROPERTY_ENABLE;
  int1_cfg.zlie = PROPERTY_ENABLE;
  int1_cfg.zhie = PROPERTY_ENABLE;
  const auto activity_mode = static_cast<uint8_t>(this->activity_interrupt_mode_);
  int1_cfg._6d = activity_mode & 0x01;
  int1_cfg.aoi = (activity_mode >> 1) & 0x01;

  if (!this->write_register_verified_(LIS3DH_INT1_CFG, *reinterpret_cast<uint8_t *>(&int1_cfg)))
    return false;
  if (lis3dh_int1_gen_threshold_set(&this->dev_ctx_, this->activity_threshold_) != 0)
    return false;
  if (lis3dh_int1_gen_duration_set(&this->dev_ctx_, this->activity_duration_) != 0)
    return false;

  // Always write the HP routing explicitly, even when disabling it -- the LIS3DH can stay
  // powered continuously across ESP32 reboots, so a stale routing left over from an earlier
  // configuration (e.g. when high_pass_filter_enabled was previously true) would otherwise
  // persist in hardware indefinitely, silently no-op'ing this option.
  const auto hp_routing = this->high_pass_filter_enabled_ ? LIS3DH_ON_INT1_GEN : LIS3DH_DISC_FROM_INT_GENERATOR;
  if (lis3dh_high_pass_int_conf_set(&this->dev_ctx_, hp_routing) != 0)
    return false;
  // Keep the output data path (and FIFO) unfiltered regardless -- OUT_X/Y/Z must carry the DC
  // component so orientation/6D zone recognition can use the gravity vector.
  if (lis3dh_high_pass_on_outputs_set(&this->dev_ctx_, PROPERTY_DISABLE) != 0)
    return false;

  if (this->high_pass_filter_enabled_) {
    uint8_t dummy;
    if (lis3dh_filter_reference_get(&this->dev_ctx_, &dummy) != 0)
      return false;
  }
  return true;
}

bool LIS3DHComponent::configure_freefall_detection_() {
  lis3dh_int2_cfg_t int2_cfg{};
  int2_cfg.xlie = PROPERTY_ENABLE;
  int2_cfg.ylie = PROPERTY_ENABLE;
  int2_cfg.zlie = PROPERTY_ENABLE;
  const auto freefall_mode = static_cast<uint8_t>(this->freefall_interrupt_mode_);
  int2_cfg._6d = freefall_mode & 0x01;
  int2_cfg.aoi = (freefall_mode >> 1) & 0x01;

  // Same INT1_CFG errata likely applies to INT2_CFG (same design family).
  if (!this->write_register_verified_(LIS3DH_INT2_CFG, *reinterpret_cast<uint8_t *>(&int2_cfg)))
    return false;
  if (lis3dh_int2_gen_threshold_set(&this->dev_ctx_, this->freefall_threshold_) != 0)
    return false;
  return lis3dh_int2_gen_duration_set(&this->dev_ctx_, this->freefall_duration_) == 0;
}

bool LIS3DHComponent::configure_auto_low_power_() {
  if (lis3dh_act_threshold_set(&this->dev_ctx_, this->auto_low_power_threshold_) != 0)
    return false;
  return lis3dh_act_timeout_set(&this->dev_ctx_, this->auto_low_power_duration_) == 0;
}

void LIS3DHComponent::read_acceleration_data_() {
  uint8_t raw_data[6];
  if (!this->read_register(LIS3DH_OUT_X_L, raw_data, 6)) {
    ESP_LOGW(TAG, "Failed to read acceleration data");
    return;
  }

  int16_t raw_x = static_cast<int16_t>((raw_data[1] << 8) | raw_data[0]);
  int16_t raw_y = static_cast<int16_t>((raw_data[3] << 8) | raw_data[2]);
  int16_t raw_z = static_cast<int16_t>((raw_data[5] << 8) | raw_data[4]);

  this->accel_x_ms2_ = this->convert_acceleration_to_ms2_(raw_x);
  this->accel_y_ms2_ = this->convert_acceleration_to_ms2_(raw_y);
  this->accel_z_ms2_ = this->convert_acceleration_to_ms2_(raw_z);

#ifdef USE_SENSOR
  if (this->acceleration_x_sensor_ != nullptr) {
    this->acceleration_x_sensor_->publish_state(this->accel_x_ms2_);
  }
  if (this->acceleration_y_sensor_ != nullptr) {
    this->acceleration_y_sensor_->publish_state(this->accel_y_ms2_);
  }
  if (this->acceleration_z_sensor_ != nullptr) {
    this->acceleration_z_sensor_->publish_state(this->accel_z_ms2_);
  }
#endif
}

void LIS3DHComponent::read_temperature_data_() {
#ifdef USE_SENSOR
  if (this->temperature_sensor_ == nullptr) {
    return;
  }
  uint8_t raw_data[2];
  if (!this->read_register(LIS3DH_OUT_ADC3_L, raw_data, 2)) {
    ESP_LOGW(TAG, "Failed to read temperature data");
    return;
  }
  int16_t temp_raw = static_cast<int16_t>((raw_data[1] << 8) | raw_data[0]);
  this->temperature_sensor_->publish_state(this->convert_temperature_(static_cast<int8_t>(temp_raw >> 8)));
#endif
}

void LIS3DHComponent::read_fifo_data_() {
  uint8_t fifo_src;
  if (!this->read_register(LIS3DH_FIFO_SRC_REG, &fifo_src, 1)) {
    return;
  }
  // Drain any pending samples so the FIFO doesn't stall — we don't aggregate
  // them, but holding state would prevent further watermark interrupts.
  const uint8_t num_samples = fifo_src & 0x1F;
  for (uint8_t i = 0; i < num_samples; i++) {
    uint8_t raw_data[6];
    if (!this->read_register(LIS3DH_OUT_X_L, raw_data, 6)) {
      break;
    }
  }
}

void LIS3DHComponent::process_event_sources_() {
  uint8_t int1_src = 0;
  uint8_t int2_src = 0;
  uint8_t click_src = 0;

  if (this->activity_enabled_) {
    this->read_register(LIS3DH_INT1_SRC, &int1_src, 1);
  }
  if (this->freefall_enabled_) {
    this->read_register(LIS3DH_INT2_SRC, &int2_src, 1);
  }
  if (this->tap_enabled_) {
    this->read_register(LIS3DH_CLICK_SRC, &click_src, 1);
  }

  const uint32_t now = millis();

  if (this->tap_enabled_) {
    if (click_src & 0x40) {  // double-tap
      if (now - this->last_double_tap_event_ > 500) {
        this->double_tap_callback_.call();
#ifdef USE_BINARY_SENSOR
        if (this->double_tap_binary_sensor_ != nullptr) {
          this->double_tap_binary_sensor_->publish_state(true);
          this->double_tap_binary_sensor_->publish_state(false);
        }
#endif
        this->last_double_tap_event_ = now;
      }
    } else if (click_src & 0x20) {  // single-tap
      if (now - this->last_tap_event_ > 500) {
        this->tap_callback_.call();
#ifdef USE_BINARY_SENSOR
        if (this->tap_binary_sensor_ != nullptr) {
          this->tap_binary_sensor_->publish_state(true);
          this->tap_binary_sensor_->publish_state(false);
        }
#endif
        this->last_tap_event_ = now;
      }
    }
  }

  if (this->activity_enabled_) {
    const bool active = (int1_src & 0x40) != 0;  // IA bit
    if (this->last_activity_state_ != active) {
      if (active) {
        this->activity_callback_.call();
      }
#ifdef USE_BINARY_SENSOR
      if (this->activity_binary_sensor_ != nullptr) {
        this->activity_binary_sensor_->publish_state(active);
      }
#endif
      this->last_activity_state_ = active;
    }
  }

  if (this->freefall_enabled_) {
    const bool freefall = (int2_src & 0x40) != 0;
    if (this->last_freefall_state_ != freefall) {
      if (freefall) {
        this->freefall_callback_.call();
      }
#ifdef USE_BINARY_SENSOR
      if (this->freefall_binary_sensor_ != nullptr) {
        this->freefall_binary_sensor_->publish_state(freefall);
      }
#endif
      this->last_freefall_state_ = freefall;
    }
  }
}

void LIS3DHComponent::update_orientation_() {
#ifdef USE_TEXT_SENSOR
  if (this->orientation_xy_text_sensor_ == nullptr && this->orientation_z_text_sensor_ == nullptr) {
    return;
  }
#else
  if (this->orientation_change_callback_.size() == 0) {
    return;
  }
#endif

  // Compare absolute axis magnitudes; the dominant axis points along gravity.
  const float ax = std::fabs(this->accel_x_ms2_);
  const float ay = std::fabs(this->accel_y_ms2_);
  const float az = std::fabs(this->accel_z_ms2_);
  // Hysteresis: require >0.6g (~5.9 m/s²) along the dominant axis to commit.
  static constexpr float MIN_DOMINANT_MS2 = 5.9f;

  LIS3DHOrientationXY xy = this->last_orientation_xy_;
  if (az > ax && az > ay) {
    xy = LIS3DHOrientationXY::FLAT;
  } else if (ay > ax && ay > MIN_DOMINANT_MS2) {
    xy = this->accel_y_ms2_ > 0 ? LIS3DHOrientationXY::PORTRAIT_UPRIGHT : LIS3DHOrientationXY::PORTRAIT_UPSIDE_DOWN;
  } else if (ax > MIN_DOMINANT_MS2) {
    xy = this->accel_x_ms2_ > 0 ? LIS3DHOrientationXY::LANDSCAPE_RIGHT : LIS3DHOrientationXY::LANDSCAPE_LEFT;
  }

  LIS3DHOrientationZ z = this->accel_z_ms2_ >= 0 ? LIS3DHOrientationZ::FACE_UP : LIS3DHOrientationZ::FACE_DOWN;

  const bool changed =
      !this->orientation_published_ || xy != this->last_orientation_xy_ || z != this->last_orientation_z_;

#ifdef USE_TEXT_SENSOR
  if (this->orientation_xy_text_sensor_ != nullptr &&
      (!this->orientation_published_ || xy != this->last_orientation_xy_)) {
    this->orientation_xy_text_sensor_->publish_state(orientation_xy_to_str(xy));
  }
  if (this->orientation_z_text_sensor_ != nullptr &&
      (!this->orientation_published_ || z != this->last_orientation_z_)) {
    this->orientation_z_text_sensor_->publish_state(orientation_z_to_str(z));
  }
#endif

  if (changed && this->orientation_published_) {
    this->orientation_change_callback_.call();
  }

  this->last_orientation_xy_ = xy;
  this->last_orientation_z_ = z;
  this->orientation_published_ = true;
}

float LIS3DHComponent::convert_acceleration_to_ms2_(int16_t raw) {
  float conversion_factor = 0.0f;

  switch (this->mode_) {
    case LIS3DHMode::MODE_LOW_POWER:
      switch (this->range_) {
        case LIS3DHRange::RANGE_2G:
          conversion_factor = lis3dh_from_fs2_lp_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_4G:
          conversion_factor = lis3dh_from_fs4_lp_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_8G:
          conversion_factor = lis3dh_from_fs8_lp_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_16G:
          conversion_factor = lis3dh_from_fs16_lp_to_mg(1.0f);
          break;
      }
      break;
    case LIS3DHMode::MODE_NORMAL:
      switch (this->range_) {
        case LIS3DHRange::RANGE_2G:
          conversion_factor = lis3dh_from_fs2_nm_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_4G:
          conversion_factor = lis3dh_from_fs4_nm_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_8G:
          conversion_factor = lis3dh_from_fs8_nm_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_16G:
          conversion_factor = lis3dh_from_fs16_nm_to_mg(1.0f);
          break;
      }
      break;
    case LIS3DHMode::MODE_HIGH_RESOLUTION:
      switch (this->range_) {
        case LIS3DHRange::RANGE_2G:
          conversion_factor = lis3dh_from_fs2_hr_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_4G:
          conversion_factor = lis3dh_from_fs4_hr_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_8G:
          conversion_factor = lis3dh_from_fs8_hr_to_mg(1.0f);
          break;
        case LIS3DHRange::RANGE_16G:
          conversion_factor = lis3dh_from_fs16_hr_to_mg(1.0f);
          break;
      }
      break;
  }

  return (raw * conversion_factor / 1000.0f) * 9.80665f;
}

float LIS3DHComponent::convert_temperature_(int16_t raw) { return 25.0f + raw / 64.0f; }

}  // namespace lis3dh
}  // namespace esphome

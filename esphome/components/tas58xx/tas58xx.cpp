#include "tas58xx.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tas58xx {

static const char *const TAG = "tas58xx";

static constexpr uint8_t TAS58XX_PAGE_SELECT = 0x00;  // Page Select, in every book
static constexpr uint8_t TAS58XX_BOOK_SELECT = 0x7F;  // Book Select, on page 0 of every book
static constexpr uint8_t TAS58XX_BOOK_CONTROL = 0x00;
static constexpr uint8_t TAS58XX_PAGE_0 = 0x00;

/* BOOK 0x00, PAGE 0x00 */
static constexpr uint8_t TAS58XX_RESET_CTRL = 0x01;
static constexpr uint8_t TAS58XX_RESET_CTRL_ALL = 0x11;  // Reset DSP/control port and registers
static constexpr uint8_t TAS58XX_DEVICE_CTRL_1 = 0x02;
static constexpr uint8_t TAS58XX_DEVICE_CTRL_1_PBTL = (1 << 2);
static constexpr uint8_t TAS58XX_DEVICE_CTRL_2 = 0x03;
static constexpr uint8_t TAS58XX_DEVICE_CTRL_2_MUTE = (1 << 3);
static constexpr uint8_t TAS58XX_CTRL_STATE_MASK = 0x03;  // DEVICE_CTRL_2 and POWER_STATE
static constexpr uint8_t TAS58XX_CTRL_STATE_DEEP_SLEEP = 0x00;
static constexpr uint8_t TAS58XX_CTRL_STATE_SLEEP = 0x01;
static constexpr uint8_t TAS58XX_CTRL_STATE_HIZ = 0x02;
static constexpr uint8_t TAS58XX_CTRL_STATE_PLAY = 0x03;
static constexpr uint8_t TAS58XX_CTRL_STATE_UNKNOWN = 0xFF;  // Not a device value, forces the next update to act
static constexpr uint8_t TAS58XX_DIG_VOL = 0x4C;  // 0x00 = +24 dB to 0xFE = -103 dB in 0.5 dB steps, 0xFF = mute
static constexpr uint8_t TAS58XX_DIG_VOL_0DB = 0x30;
static constexpr uint8_t TAS58XX_DIG_VOL_MINUS_103DB = 0xFE;
static constexpr uint8_t TAS58XX_AGAIN = 0x54;  // 0x00 = 0 dB to 0x1F = -15.5 dB in 0.5 dB steps
static constexpr uint8_t TAS58XX_AGAIN_MINUS_15_5DB = 0x1F;
static constexpr uint8_t TAS58XX_POWER_STATE = 0x68;
static constexpr uint8_t TAS58XX_CHAN_FAULT = 0x70;  // GLOBAL_FAULT1, GLOBAL_FAULT2 and OT_WARNING follow
static constexpr uint8_t TAS58XX_FAULT_REGISTER_COUNT = 4;
static constexpr uint8_t TAS58XX_FAULT_CLEAR = 0x78;
static constexpr uint8_t TAS58XX_FAULT_CLEAR_ANALOG = 0x80;

/* Input mixer, four 9.23 fixed point big endian coefficients at a location that differs per model */
static constexpr uint8_t TAS58XX_MIXER_COEFFICIENT_SIZE = 4;
// Only the second byte differs between mute, -6 dB and 0 dB
static constexpr uint8_t TAS58XX_MIXER_MUTE = 0x00;
static constexpr uint8_t TAS58XX_MIXER_MINUS_6DB = 0x40;
static constexpr uint8_t TAS58XX_MIXER_0DB = 0x80;

static constexpr uint32_t PDN_LOW_MS = 1;
static constexpr uint32_t PDN_TO_I2C_MS = 5;  // Minimum time from PDN high to I2C access
static constexpr uint32_t RESET_SETTLE_MS = 5;

static const LogString *power_state_name(uint8_t state) {
  if (state == TAS58XX_CTRL_STATE_DEEP_SLEEP)
    return LOG_STR("Deep sleep");
  if (state == TAS58XX_CTRL_STATE_SLEEP)
    return LOG_STR("Sleep");
  if (state == TAS58XX_CTRL_STATE_HIZ)
    return LOG_STR("Hi-Z");
  return LOG_STR("Play");
}

void TAS58xx::setup() {
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(false);
    delay(PDN_LOW_MS);
    this->enable_pin_->digital_write(true);
    delay(PDN_TO_I2C_MS);
  }
  if (!this->init_()) {
    this->mark_failed();
  }
}

bool TAS58xx::select_book_page_(uint8_t book, uint8_t page) {
  // The book can only be changed from page 0
  return this->write_byte(TAS58XX_PAGE_SELECT, TAS58XX_PAGE_0) && this->write_byte(TAS58XX_BOOK_SELECT, book) &&
         this->write_byte(TAS58XX_PAGE_SELECT, page);
}

bool TAS58xx::init_() {
  // Header of every TI PurePath Console export: select book 0 in case the MCU restarted without a PDN toggle,
  // silence the output with Hi-Z, reset the DSP and the control registers, then return to Hi-Z
  if (!this->select_book_page_(TAS58XX_BOOK_CONTROL, TAS58XX_PAGE_0) ||
      !this->write_byte(TAS58XX_DEVICE_CTRL_2, TAS58XX_CTRL_STATE_HIZ) ||
      !this->write_byte(TAS58XX_RESET_CTRL, TAS58XX_RESET_CTRL_ALL) ||
      !this->write_byte(TAS58XX_DEVICE_CTRL_2, TAS58XX_CTRL_STATE_HIZ)) {
    ESP_LOGE(TAG, "I2C write failed during reset");
    return false;
  }
  delay(RESET_SETTLE_MS);

  for (uint8_t i = 0; i < this->model_->startup_sequence_length; i++) {
    const uint8_t *entry = this->model_->startup_sequence[i];
    if (!this->write_byte(progmem_read_byte(&entry[0]), progmem_read_byte(&entry[1]))) {
      ESP_LOGE(TAG, "I2C write failed during init");
      return false;
    }
  }

  // Setters are public, so keep the value inside the register range
  const uint8_t again =
      static_cast<uint8_t>(lroundf(clamp(-this->analog_gain_db_ * 2.0f, 0.0f, float{TAS58XX_AGAIN_MINUS_15_5DB})));
  if (!this->write_byte(TAS58XX_DEVICE_CTRL_1, this->dac_mode_ == DAC_MODE_PBTL ? TAS58XX_DEVICE_CTRL_1_PBTL : 0) ||
      !this->write_byte(TAS58XX_AGAIN, again) || !this->write_volume_() ||
      !this->write_ctrl_state_(TAS58XX_CTRL_STATE_PLAY, this->is_muted_) ||
      !this->write_byte(TAS58XX_FAULT_CLEAR, TAS58XX_FAULT_CLEAR_ANALOG)) {
    ESP_LOGE(TAG, "I2C write failed during init");
    return false;
  }
  // The mixer is written once the I2S clocks run, see on_audio_started() and update()
  this->power_state_ = TAS58XX_CTRL_STATE_UNKNOWN;
  this->mixer_written_ = false;
  return true;
}

void TAS58xx::activate() {
  if (this->is_failed())
    return;
  ESP_LOGD(TAG, "[0x%02X] Activating", this->address_);
  // A failed mixer write can leave the device in another book
  if (!this->select_book_page_(TAS58XX_BOOK_CONTROL, TAS58XX_PAGE_0)) {
    ESP_LOGE(TAG, "Failed to select the control port");
    return;
  }
  // Also the way to restart the output after a DC or over current fault
  if (!this->write_byte(TAS58XX_FAULT_CLEAR, TAS58XX_FAULT_CLEAR_ANALOG)) {
    ESP_LOGW(TAG, "Failed to clear faults");
  }
  const bool muted = this->is_muted_;
  // Leaving deep sleep needs this sequence to reset the internal state machine, see datasheet 7.4.5
  if (this->ctrl_state_ == TAS58XX_CTRL_STATE_DEEP_SLEEP &&
      !(this->write_ctrl_state_(TAS58XX_CTRL_STATE_HIZ, muted) &&
        this->write_ctrl_state_(TAS58XX_CTRL_STATE_DEEP_SLEEP, muted) &&
        this->write_ctrl_state_(TAS58XX_CTRL_STATE_HIZ, muted))) {
    return;
  }
  this->write_ctrl_state_(TAS58XX_CTRL_STATE_PLAY, muted);
}

void TAS58xx::deactivate() {
  if (this->is_failed())
    return;
  ESP_LOGD(TAG, "[0x%02X] Deactivating", this->address_);
  if (!this->select_book_page_(TAS58XX_BOOK_CONTROL, TAS58XX_PAGE_0)) {
    ESP_LOGE(TAG, "Failed to select the control port");
    return;
  }
  // The DSP, and with it the mixer, stays active in deep sleep
  this->write_ctrl_state_(TAS58XX_CTRL_STATE_DEEP_SLEEP, this->is_muted_);
}

bool TAS58xx::write_ctrl_state_(uint8_t state, bool muted) {
  if (!this->write_byte(TAS58XX_DEVICE_CTRL_2, state | (muted ? TAS58XX_DEVICE_CTRL_2_MUTE : 0))) {
    ESP_LOGE(TAG, "Failed to write DEVICE_CTRL_2");
    return false;
  }
  this->ctrl_state_ = state;
  return true;
}

// Returns false if the fault registers could not be read
bool TAS58xx::read_faults_() {
  uint8_t fault_registers[TAS58XX_FAULT_REGISTER_COUNT];
  if (!this->read_bytes(TAS58XX_CHAN_FAULT, fault_registers, sizeof(fault_registers)))
    return false;
  uint32_t faults = 0;
  for (uint8_t reg = 0; reg < TAS58XX_FAULT_REGISTER_COUNT; reg++)
    faults |= uint32_t{fault_registers[reg]} << (reg * 8);
  const ModelInfo &model = *this->model_;
  const uint32_t active = faults & (model.fault_error_mask | model.fault_warning_mask);

  // Clearing makes a lasting condition latch again on every poll, so only log changes
  const uint32_t changed = active ^ this->logged_faults_;
  for (uint8_t index = 0; index < 32; index++) {
    const uint32_t mask = uint32_t{1} << index;
    if (!(changed & mask))
      continue;
    const LogString *name = model.fault_name(index);
    if (!(active & mask)) {
      ESP_LOGI(TAG, "[0x%02X] %s cleared", this->address_, LOG_STR_ARG(name));
    } else if (model.fault_error_mask & mask) {
      ESP_LOGE(TAG, "[0x%02X] %s", this->address_, LOG_STR_ARG(name));
    } else {
      ESP_LOGW(TAG, "[0x%02X] %s", this->address_, LOG_STR_ARG(name));
    }
  }
  if (changed & active & model.fault_output_off_mask) {
    ESP_LOGW(TAG, "[0x%02X] Output stays off: fix the cause, then call tas58xx.activate or power cycle the amplifier",
             this->address_);
  }
  this->logged_faults_ = active;

#ifdef USE_BINARY_SENSOR
  if (this->have_fault_binary_sensor_ != nullptr)
    this->have_fault_binary_sensor_->publish_state((active & model.fault_error_mask) != 0);
  for (uint8_t fault = 0; fault < FAULT_SENSOR_COUNT; fault++) {
    const uint8_t bit = model.fault_sensor_bits[fault];
    if (this->fault_binary_sensors_[fault] != nullptr && bit != NO_BIT)
      this->fault_binary_sensors_[fault]->publish_state(faults & (uint32_t{1} << bit));
  }
#endif

  // Latched fault bits stay set after the condition is gone and only report it; clear those so the bits, and the
  // sensors, follow the current state. The clear register resets all faults at once, so hold off while the output
  // is off.
  if ((faults & model.fault_latched_mask) && !(active & model.fault_output_off_mask) &&
      !this->write_byte(TAS58XX_FAULT_CLEAR, TAS58XX_FAULT_CLEAR_ANALOG)) {
    ESP_LOGW(TAG, "Failed to clear faults");
  }
  return true;
}

void TAS58xx::on_audio_started() {
  // DSP coefficients can only be written with a running I2S clock (datasheet 7.5.3.1) and are kept when it stops
  if (this->is_failed() || this->mixer_written_)
    return;
  if (!this->write_mixer_()) {
    ESP_LOGW(TAG, "Failed to write mixer");
  }
}

void TAS58xx::update() {
  uint8_t power_state;
  if (!this->read_faults_() || !this->read_byte(TAS58XX_POWER_STATE, &power_state)) {
    this->status_set_warning(LOG_STR("Failed to read status"));
    return;
  }
  this->status_clear_warning();

  power_state &= TAS58XX_CTRL_STATE_MASK;
  if (power_state != this->power_state_) {
    ESP_LOGD(TAG, "[0x%02X] Power state: %s", this->address_, LOG_STR_ARG(power_state_name(power_state)));
    this->power_state_ = power_state;
  }
  // The device only plays with a running I2S clock. Fallback for speakers that do not call on_audio_started();
  // a failed write is retried on the next update.
  if (power_state == TAS58XX_CTRL_STATE_PLAY && !this->mixer_written_ && !this->write_mixer_()) {
    ESP_LOGW(TAG, "Failed to write mixer");
  }
}

void TAS58xx::dump_config() {
  const LogString *mixer_mode;
  if (this->mixer_mode_ == MIXER_MODE_STEREO_INVERSE) {
    mixer_mode = LOG_STR("Stereo inverse");
  } else if (this->mixer_mode_ == MIXER_MODE_MONO) {
    mixer_mode = LOG_STR("Mono");
  } else if (this->mixer_mode_ == MIXER_MODE_LEFT) {
    mixer_mode = LOG_STR("Left");
  } else if (this->mixer_mode_ == MIXER_MODE_RIGHT) {
    mixer_mode = LOG_STR("Right");
  } else {
    mixer_mode = LOG_STR("Stereo");
  }
  ESP_LOGCONFIG(TAG,
                "Audio Amplifier:\n"
                "  Model: %s",
                LOG_STR_ARG(this->model_->name()));
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Analog Gain: %.1f dB\n"
                "  DAC Mode: %s\n"
                "  Mixer Mode: %s\n"
                "  Volume Range: %.1f dB to %.1f dB",
                this->analog_gain_db_,
                this->dac_mode_ == DAC_MODE_PBTL ? LOG_STR_LITERAL("PBTL") : LOG_STR_LITERAL("BTL"),
                LOG_STR_ARG(mixer_mode), this->volume_min_db_, this->volume_max_db_);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Any Fault", this->have_fault_binary_sensor_);
  for (auto *sensor : this->fault_binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Fault", sensor);
  }
#endif
}

bool TAS58xx::set_mute_(bool muted) {
  if (!this->write_ctrl_state_(this->ctrl_state_, muted))
    return false;
  this->is_muted_ = muted;
  return true;
}

bool TAS58xx::set_volume(float volume) {
  float previous = this->volume_;
  this->volume_ = clamp(volume, 0.0f, 1.0f);
  if (!this->write_volume_()) {
    this->volume_ = previous;
    return false;
  }
  return true;
}

bool TAS58xx::write_volume_() {
  // volume 0.0 maps to volume_min_db_, which is only close to silence at -103 dB
  const float volume_db = std::lerp(this->volume_min_db_, this->volume_max_db_, this->volume_);
  const uint8_t dig_vol = static_cast<uint8_t>(
      lroundf(clamp(TAS58XX_DIG_VOL_0DB - volume_db * 2.0f, 0.0f, float{TAS58XX_DIG_VOL_MINUS_103DB})));
  ESP_LOGV(TAG, "Setting volume to 0x%02X", dig_vol);
  return this->write_byte(TAS58XX_DIG_VOL, dig_vol);
}

bool TAS58xx::write_mixer_() {
  uint8_t left_to_left = TAS58XX_MIXER_0DB;
  uint8_t right_to_left = TAS58XX_MIXER_MUTE;
  uint8_t left_to_right = TAS58XX_MIXER_MUTE;
  uint8_t right_to_right = TAS58XX_MIXER_0DB;
  if (this->mixer_mode_ == MIXER_MODE_STEREO_INVERSE) {
    left_to_left = TAS58XX_MIXER_MUTE;
    right_to_left = TAS58XX_MIXER_0DB;
    left_to_right = TAS58XX_MIXER_0DB;
    right_to_right = TAS58XX_MIXER_MUTE;
  } else if (this->mixer_mode_ == MIXER_MODE_MONO) {
    left_to_left = TAS58XX_MIXER_MINUS_6DB;
    right_to_left = TAS58XX_MIXER_MINUS_6DB;
    left_to_right = TAS58XX_MIXER_MINUS_6DB;
    right_to_right = TAS58XX_MIXER_MINUS_6DB;
  } else if (this->mixer_mode_ == MIXER_MODE_LEFT) {
    left_to_right = TAS58XX_MIXER_0DB;
    right_to_right = TAS58XX_MIXER_MUTE;
  } else if (this->mixer_mode_ == MIXER_MODE_RIGHT) {
    left_to_left = TAS58XX_MIXER_MUTE;
    right_to_left = TAS58XX_MIXER_0DB;
  }
  const uint8_t coefficients[4 * TAS58XX_MIXER_COEFFICIENT_SIZE] = {0, left_to_left,  0, 0, 0, right_to_left,  0, 0,
                                                                    0, left_to_right, 0, 0, 0, right_to_right, 0, 0};
  bool ok = this->select_book_page_(this->model_->mixer_book, this->model_->mixer_page) &&
            this->write_bytes(this->model_->mixer_register, coefficients, sizeof(coefficients));
  // Always return to the control port, even after a failed write
  ok = this->select_book_page_(TAS58XX_BOOK_CONTROL, TAS58XX_PAGE_0) && ok;
  this->mixer_written_ = ok;
  return ok;
}

}  // namespace esphome::tas58xx

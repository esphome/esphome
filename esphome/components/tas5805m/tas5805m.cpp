#include "tas5805m.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tas5805m {

static const char *const TAG = "tas5805m";

static constexpr uint8_t TAS5805M_PAGE_SELECT = 0x00;  // Page Select, in every book
static constexpr uint8_t TAS5805M_BOOK_SELECT = 0x7F;  // Book Select, on page 0 of every book
static constexpr uint8_t TAS5805M_BOOK_CONTROL = 0x00;
static constexpr uint8_t TAS5805M_PAGE_0 = 0x00;
static constexpr uint8_t TAS5805M_PAGE_1 = 0x01;

/* BOOK 0x00, PAGE 0x00 */
static constexpr uint8_t TAS5805M_RESET_CTRL = 0x01;
static constexpr uint8_t TAS5805M_RESET_CTRL_ALL = 0x11;  // Reset DSP/control port and registers
static constexpr uint8_t TAS5805M_DEVICE_CTRL_1 = 0x02;
static constexpr uint8_t TAS5805M_DEVICE_CTRL_1_PBTL = (1 << 2);
static constexpr uint8_t TAS5805M_DEVICE_CTRL_2 = 0x03;
static constexpr uint8_t TAS5805M_DEVICE_CTRL_2_MUTE = (1 << 3);
static constexpr uint8_t TAS5805M_CTRL_STATE_MASK = 0x03;  // DEVICE_CTRL_2 and POWER_STATE
static constexpr uint8_t TAS5805M_CTRL_STATE_DEEP_SLEEP = 0x00;
static constexpr uint8_t TAS5805M_CTRL_STATE_SLEEP = 0x01;
static constexpr uint8_t TAS5805M_CTRL_STATE_HIZ = 0x02;
static constexpr uint8_t TAS5805M_CTRL_STATE_PLAY = 0x03;
static constexpr uint8_t TAS5805M_CTRL_STATE_UNKNOWN = 0xFF;  // Not a device value, forces the next update to act
static constexpr uint8_t TAS5805M_DIG_VOL = 0x4C;  // 0x00 = +24 dB to 0xFE = -103 dB in 0.5 dB steps, 0xFF = mute
static constexpr uint8_t TAS5805M_DIG_VOL_0DB = 0x30;
static constexpr uint8_t TAS5805M_DIG_VOL_MINUS_103DB = 0xFE;
static constexpr uint8_t TAS5805M_AGAIN = 0x54;  // 0x00 = 0 dB to 0x1F = -15.5 dB in 0.5 dB steps
static constexpr uint8_t TAS5805M_AGAIN_MINUS_15_5DB = 0x1F;
static constexpr uint8_t TAS5805M_ADR_PIN_CTRL = 0x60;
static constexpr uint8_t TAS5805M_ADR_PIN_CTRL_OUTPUT = 0x01;
static constexpr uint8_t TAS5805M_ADR_PIN_CONFIG = 0x61;
static constexpr uint8_t TAS5805M_ADR_PIN_CONFIG_FAULTZ = 0x0B;
static constexpr uint8_t TAS5805M_POWER_STATE = 0x68;
static constexpr uint8_t TAS5805M_CHAN_FAULT = 0x70;  // GLOBAL_FAULT1, GLOBAL_FAULT2 and OT_WARNING follow
static constexpr uint8_t TAS5805M_FAULT_REGISTER_COUNT = 4;
static constexpr uint8_t TAS5805M_FAULT_CLEAR = 0x78;
static constexpr uint8_t TAS5805M_FAULT_CLEAR_ANALOG = 0x80;

/* BOOK 0x8C, PAGE 0x29: input mixer, four 9.23 fixed point big endian coefficients */
static constexpr uint8_t TAS5805M_BOOK_MIXER = 0x8C;
static constexpr uint8_t TAS5805M_PAGE_MIXER = 0x29;
static constexpr uint8_t TAS5805M_MIXER_LEFT_TO_LEFT = 0x18;  // RIGHT_TO_LEFT, LEFT_TO_RIGHT, RIGHT_TO_RIGHT follow
static constexpr uint8_t TAS5805M_MIXER_COEFFICIENT_SIZE = 4;
// Only the second byte differs between mute, -6 dB and 0 dB
static constexpr uint8_t TAS5805M_MIXER_MUTE = 0x00;
static constexpr uint8_t TAS5805M_MIXER_MINUS_6DB = 0x40;
static constexpr uint8_t TAS5805M_MIXER_0DB = 0x80;

static constexpr uint32_t PDN_LOW_MS = 10;
static constexpr uint32_t PDN_TO_I2C_MS = 5;  // Minimum time from PDN high to I2C access
static constexpr uint32_t RESET_SETTLE_MS = 5;

// Remainder of the startup sequence from TI PurePath Console, run after the reset. Register 0x00 selects the page.
// Registers 0x46, 0x7D, 0x7E and page 1 register 0x51 are not documented in the datasheet.
static const uint8_t STARTUP_SEQUENCE[][2] PROGMEM = {
    {TAS5805M_DEVICE_CTRL_2, TAS5805M_CTRL_STATE_DEEP_SLEEP},
    {0x46, 0x01},
    {TAS5805M_DEVICE_CTRL_2, TAS5805M_CTRL_STATE_HIZ},
    // The I2C address is latched at power up, after which the ADR pin can report faults
    {TAS5805M_ADR_PIN_CONFIG, TAS5805M_ADR_PIN_CONFIG_FAULTZ},
    {TAS5805M_ADR_PIN_CTRL, TAS5805M_ADR_PIN_CTRL_OUTPUT},
    {0x7D, 0x11},
    {0x7E, 0xFF},
    {TAS5805M_PAGE_SELECT, TAS5805M_PAGE_1},
    {0x51, 0x05},
    {TAS5805M_PAGE_SELECT, TAS5805M_PAGE_0},
};

// Fault bits per register (CHAN_FAULT, GLOBAL_FAULT1, GLOBAL_FAULT2, OT_WARNING, one byte each, low to high).
// The clock fault is left out of the log and have_fault: it is set whenever the I2S clock stops, which is normal.
static constexpr uint32_t TAS5805M_FAULT_ERROR_MASKS = 0x0001C30F;
static constexpr uint32_t TAS5805M_FAULT_WARNING_MASKS = 0x04000000;
static constexpr uint8_t TAS5805M_GLOBAL_FAULT1_CLOCK_BIT = 2;

// An if chain rather than a switch: a switch table would land in rodata, which is RAM on ESP8266.
static const LogString *fault_name(uint8_t reg, uint8_t bit) {
  const uint8_t key = (reg << 3) | bit;
  if (key == 0x00)
    return LOG_STR("Right channel over current");
  if (key == 0x01)
    return LOG_STR("Left channel over current");
  if (key == 0x02)
    return LOG_STR("Right channel DC fault");
  if (key == 0x03)
    return LOG_STR("Left channel DC fault");
  if (key == 0x08)
    return LOG_STR("PVDD under voltage");
  if (key == 0x09)
    return LOG_STR("PVDD over voltage");
  if (key == 0x0E)
    return LOG_STR("BQ write failed");
  if (key == 0x0F)
    return LOG_STR("OTP CRC check error");
  if (key == 0x10)
    return LOG_STR("Over temperature shutdown");
  if (key == 0x1A)
    return LOG_STR("Over temperature warning");
  return nullptr;
}

static const LogString *power_state_name(uint8_t state) {
  if (state == TAS5805M_CTRL_STATE_DEEP_SLEEP)
    return LOG_STR("Deep sleep");
  if (state == TAS5805M_CTRL_STATE_SLEEP)
    return LOG_STR("Sleep");
  if (state == TAS5805M_CTRL_STATE_HIZ)
    return LOG_STR("Hi-Z");
  return LOG_STR("Play");
}

void TAS5805M::setup() {
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

bool TAS5805M::select_book_page_(uint8_t book, uint8_t page) {
  // The book can only be changed from page 0
  return this->write_byte(TAS5805M_PAGE_SELECT, TAS5805M_PAGE_0) && this->write_byte(TAS5805M_BOOK_SELECT, book) &&
         this->write_byte(TAS5805M_PAGE_SELECT, page);
}

bool TAS5805M::init_() {
  if (!this->select_book_page_(TAS5805M_BOOK_CONTROL, TAS5805M_PAGE_0) ||
      !this->write_byte(TAS5805M_DEVICE_CTRL_2, TAS5805M_CTRL_STATE_HIZ) ||
      !this->write_byte(TAS5805M_RESET_CTRL, TAS5805M_RESET_CTRL_ALL) ||
      !this->write_byte(TAS5805M_DEVICE_CTRL_2, TAS5805M_CTRL_STATE_HIZ)) {
    ESP_LOGE(TAG, "I2C write failed during reset");
    return false;
  }
  delay(RESET_SETTLE_MS);

  for (const auto &entry : STARTUP_SEQUENCE) {
    if (!this->write_byte(progmem_read_byte(&entry[0]), progmem_read_byte(&entry[1]))) {
      ESP_LOGE(TAG, "I2C write failed during init");
      return false;
    }
  }

  // Setters are public, so keep the value inside the register range
  const uint8_t again =
      static_cast<uint8_t>(lroundf(clamp(-this->analog_gain_db_ * 2.0f, 0.0f, float{TAS5805M_AGAIN_MINUS_15_5DB})));
  if (!this->write_byte(TAS5805M_DEVICE_CTRL_1, this->dac_mode_ == DAC_MODE_PBTL ? TAS5805M_DEVICE_CTRL_1_PBTL : 0) ||
      !this->write_byte(TAS5805M_AGAIN, again) || !this->write_volume_() ||
      !this->write_ctrl_state_(TAS5805M_CTRL_STATE_PLAY, this->is_muted_) ||
      !this->write_byte(TAS5805M_FAULT_CLEAR, TAS5805M_FAULT_CLEAR_ANALOG)) {
    ESP_LOGE(TAG, "I2C write failed during init");
    return false;
  }
  // The mixer is written once the device reaches play, see update()
  this->power_state_ = TAS5805M_CTRL_STATE_UNKNOWN;
  return true;
}

void TAS5805M::activate() {
  if (this->is_failed())
    return;
  ESP_LOGD(TAG, "Activating");
  const bool muted = this->is_muted_;
  // Leaving deep sleep needs this sequence to reset the internal state machine, see datasheet 7.4.5
  if (this->ctrl_state_ == TAS5805M_CTRL_STATE_DEEP_SLEEP &&
      !(this->write_ctrl_state_(TAS5805M_CTRL_STATE_HIZ, muted) &&
        this->write_ctrl_state_(TAS5805M_CTRL_STATE_DEEP_SLEEP, muted) &&
        this->write_ctrl_state_(TAS5805M_CTRL_STATE_HIZ, muted))) {
    return;
  }
  this->write_ctrl_state_(TAS5805M_CTRL_STATE_PLAY, muted);
}

void TAS5805M::deactivate() {
  if (this->is_failed())
    return;
  ESP_LOGD(TAG, "Deactivating");
  // The DSP, and with it the mixer, stays active in deep sleep
  this->write_ctrl_state_(TAS5805M_CTRL_STATE_DEEP_SLEEP, this->is_muted_);
}

bool TAS5805M::write_ctrl_state_(uint8_t state, bool muted) {
  if (!this->write_byte(TAS5805M_DEVICE_CTRL_2, state | (muted ? TAS5805M_DEVICE_CTRL_2_MUTE : 0))) {
    ESP_LOGE(TAG, "Failed to write DEVICE_CTRL_2");
    return false;
  }
  this->ctrl_state_ = state;
  return true;
}

#ifdef USE_BINARY_SENSOR
static void publish_fault(binary_sensor::BinarySensor *sensor, const uint8_t *faults, uint8_t reg, uint8_t bit) {
  if (sensor != nullptr)
    sensor->publish_state(faults[reg] & (1 << bit));
}
#endif

// Returns false if the fault registers could not be read
bool TAS5805M::read_faults_() {
  uint8_t faults[TAS5805M_FAULT_REGISTER_COUNT];
  if (!this->read_bytes(TAS5805M_CHAN_FAULT, faults, sizeof(faults)))
    return false;
  uint32_t active = 0;
  for (uint8_t reg = 0; reg < TAS5805M_FAULT_REGISTER_COUNT; reg++)
    active |= uint32_t{faults[reg]} << (reg * 8);
  active &= TAS5805M_FAULT_ERROR_MASKS | TAS5805M_FAULT_WARNING_MASKS;

  // Clearing makes a lasting condition latch again on every poll, so only log changes
  const uint32_t changed = active ^ this->logged_faults_;
  for (uint8_t index = 0; index < 32; index++) {
    const uint32_t mask = uint32_t{1} << index;
    if (!(changed & mask))
      continue;
    const LogString *name = fault_name(index / 8, index % 8);
    if (!(active & mask)) {
      ESP_LOGI(TAG, "%s cleared", LOG_STR_ARG(name));
    } else if (TAS5805M_FAULT_ERROR_MASKS & mask) {
      ESP_LOGE(TAG, "%s", LOG_STR_ARG(name));
    } else {
      ESP_LOGW(TAG, "%s", LOG_STR_ARG(name));
    }
  }
  this->logged_faults_ = active;

#ifdef USE_BINARY_SENSOR
  if (this->have_fault_binary_sensor_ != nullptr)
    this->have_fault_binary_sensor_->publish_state((active & TAS5805M_FAULT_ERROR_MASKS) != 0);
  publish_fault(this->right_channel_over_current_binary_sensor_, faults, 0, 0);
  publish_fault(this->left_channel_over_current_binary_sensor_, faults, 0, 1);
  publish_fault(this->right_channel_dc_fault_binary_sensor_, faults, 0, 2);
  publish_fault(this->left_channel_dc_fault_binary_sensor_, faults, 0, 3);
  publish_fault(this->pvdd_under_voltage_binary_sensor_, faults, 1, 0);
  publish_fault(this->pvdd_over_voltage_binary_sensor_, faults, 1, 1);
  publish_fault(this->clock_fault_binary_sensor_, faults, 1, TAS5805M_GLOBAL_FAULT1_CLOCK_BIT);
  publish_fault(this->bq_write_failed_binary_sensor_, faults, 1, 6);
  publish_fault(this->otp_crc_check_binary_sensor_, faults, 1, 7);
  publish_fault(this->over_temp_shutdown_binary_sensor_, faults, 2, 0);
  publish_fault(this->over_temp_warning_binary_sensor_, faults, 3, 2);
#endif

  // Fault bits stay set until cleared, even after the condition is gone; a DC fault also keeps the output off
  // until then (datasheet 7.5.3.3). Clearing the clock fault too keeps every bit a reflection of the current state.
  const bool clock_fault = faults[1] & (1 << TAS5805M_GLOBAL_FAULT1_CLOCK_BIT);
  if ((active != 0 || clock_fault) && !this->write_byte(TAS5805M_FAULT_CLEAR, TAS5805M_FAULT_CLEAR_ANALOG)) {
    ESP_LOGW(TAG, "Failed to clear faults");
  }
  return true;
}

void TAS5805M::update() {
  uint8_t power_state;
  if (!this->read_faults_() || !this->read_byte(TAS5805M_POWER_STATE, &power_state)) {
    this->status_set_warning(LOG_STR("Failed to read status"));
    return;
  }
  this->status_clear_warning();

  power_state &= TAS5805M_CTRL_STATE_MASK;
  if (power_state == this->power_state_)
    return;
  ESP_LOGD(TAG, "Power state: %s", LOG_STR_ARG(power_state_name(power_state)));
  // DSP coefficients need a running I2S clock (datasheet 7.5.3.1), which the device signals by entering play.
  // Leave power_state_ unchanged on failure so the next update retries.
  if (power_state == TAS5805M_CTRL_STATE_PLAY && !this->write_mixer_()) {
    ESP_LOGW(TAG, "Failed to write mixer");
    return;
  }
  this->power_state_ = power_state;
}

void TAS5805M::dump_config() {
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
  ESP_LOGCONFIG(TAG, "Audio Amplifier:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  Analog Gain: %.1f dB\n"
                "  DAC Mode: %s\n"
                "  Mixer Mode: %s\n"
                "  Volume Range: %.1f dB - %.1f dB",
                this->analog_gain_db_,
                this->dac_mode_ == DAC_MODE_PBTL ? LOG_STR_LITERAL("PBTL") : LOG_STR_LITERAL("BTL"),
                LOG_STR_ARG(mixer_mode), this->volume_min_db_, this->volume_max_db_);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Any Fault", this->have_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Left Channel DC Fault", this->left_channel_dc_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Right Channel DC Fault", this->right_channel_dc_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Left Channel Over Current", this->left_channel_over_current_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Right Channel Over Current", this->right_channel_over_current_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "OTP CRC Check", this->otp_crc_check_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "BQ Write Failed", this->bq_write_failed_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Clock Fault", this->clock_fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "PVDD Over Voltage", this->pvdd_over_voltage_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "PVDD Under Voltage", this->pvdd_under_voltage_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Over Temperature Shutdown", this->over_temp_shutdown_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Over Temperature Warning", this->over_temp_warning_binary_sensor_);
#endif
}

bool TAS5805M::set_mute_(bool muted) {
  if (!this->write_ctrl_state_(this->ctrl_state_, muted))
    return false;
  this->is_muted_ = muted;
  return true;
}

bool TAS5805M::set_volume(float volume) {
  float previous = this->volume_;
  this->volume_ = clamp(volume, 0.0f, 1.0f);
  if (!this->write_volume_()) {
    this->volume_ = previous;
    return false;
  }
  return true;
}

bool TAS5805M::write_volume_() {
  // volume 0.0 maps to volume_min_db_, which is only close to silence at -103 dB
  const float volume_db = std::lerp(this->volume_min_db_, this->volume_max_db_, this->volume_);
  const uint8_t dig_vol = static_cast<uint8_t>(
      lroundf(clamp(TAS5805M_DIG_VOL_0DB - volume_db * 2.0f, 0.0f, float{TAS5805M_DIG_VOL_MINUS_103DB})));
  ESP_LOGV(TAG, "Setting volume to 0x%02X", dig_vol);
  return this->write_byte(TAS5805M_DIG_VOL, dig_vol);
}

bool TAS5805M::write_mixer_() {
  uint8_t left_to_left = TAS5805M_MIXER_0DB;
  uint8_t right_to_left = TAS5805M_MIXER_MUTE;
  uint8_t left_to_right = TAS5805M_MIXER_MUTE;
  uint8_t right_to_right = TAS5805M_MIXER_0DB;
  if (this->mixer_mode_ == MIXER_MODE_STEREO_INVERSE) {
    left_to_left = TAS5805M_MIXER_MUTE;
    right_to_left = TAS5805M_MIXER_0DB;
    left_to_right = TAS5805M_MIXER_0DB;
    right_to_right = TAS5805M_MIXER_MUTE;
  } else if (this->mixer_mode_ == MIXER_MODE_MONO) {
    left_to_left = TAS5805M_MIXER_MINUS_6DB;
    right_to_left = TAS5805M_MIXER_MINUS_6DB;
    left_to_right = TAS5805M_MIXER_MINUS_6DB;
    right_to_right = TAS5805M_MIXER_MINUS_6DB;
  } else if (this->mixer_mode_ == MIXER_MODE_LEFT) {
    left_to_right = TAS5805M_MIXER_0DB;
    right_to_right = TAS5805M_MIXER_MUTE;
  } else if (this->mixer_mode_ == MIXER_MODE_RIGHT) {
    left_to_left = TAS5805M_MIXER_MUTE;
    right_to_left = TAS5805M_MIXER_0DB;
  }
  const uint8_t coefficients[4 * TAS5805M_MIXER_COEFFICIENT_SIZE] = {0, left_to_left,  0, 0, 0, right_to_left,  0, 0,
                                                                     0, left_to_right, 0, 0, 0, right_to_right, 0, 0};
  bool ok = this->select_book_page_(TAS5805M_BOOK_MIXER, TAS5805M_PAGE_MIXER) &&
            this->write_bytes(TAS5805M_MIXER_LEFT_TO_LEFT, coefficients, sizeof(coefficients));
  // Always return to the control port, even after a failed write
  return this->select_book_page_(TAS5805M_BOOK_CONTROL, TAS5805M_PAGE_0) && ok;
}

}  // namespace esphome::tas5805m

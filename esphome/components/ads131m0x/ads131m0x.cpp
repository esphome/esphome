#include "ads131m0x.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cmath>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::ads131m0x {

ESPHOME_LOG_TAG(TAG, "ads131m0x");

// Commands, 16 bit, left-aligned in a 24-bit word
static constexpr uint16_t CMD_NULL = 0x0000;
static constexpr uint16_t CMD_RREG = 0xA000;  // 101a aaaa annn nnnn
static constexpr uint16_t CMD_WREG = 0x6000;  // 011a aaaa annn nnnn
static constexpr uint8_t REG_ADDRESS_SHIFT = 7;

static constexpr uint8_t REG_ID = 0x00;
static constexpr uint8_t REG_MODE = 0x02;
static constexpr uint8_t REG_CLOCK = 0x03;
static constexpr uint8_t REG_GAIN1 = 0x04;
static constexpr uint8_t REG_GAIN2 = 0x05;

// ID: bits 15:12 = 0010, bits 11:8 = channel count
static constexpr uint16_t ID_FAMILY = 0x2;
// STATUS.RESET is set after power-up or any device reset and cleared by writing MODE.RESET = 0
static constexpr uint16_t STATUS_RESET = 1 << 10;
// MODE: 24-bit words, CCITT output CRC, no input CRC, RESET flag cleared, SPI timeout on, DRDY active low
static constexpr uint16_t MODE_VALUE = 0x0110;
static constexpr uint8_t CLOCK_CH_EN_SHIFT = 8;
static constexpr uint8_t CLOCK_OSR_SHIFT = 2;
static constexpr uint8_t GAIN_CHANNELS_PER_REGISTER = 4;

static constexpr size_t WORD_BYTES = 3;
static constexpr size_t MAX_FRAME_BYTES = (2 + MAX_CHANNELS) * WORD_BYTES;  // response + channels + CRC
static constexpr float CODE_FULL_SCALE = 8388608.0f;                        // 2^23
static constexpr float INTERNAL_REFERENCE_V = 1.2f;
static constexpr uint16_t CRC_INIT = 0xFFFF;
static constexpr uint16_t CRC_CCITT_POLY = 0x1021;

static constexpr uint32_t TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 5;  // above the main loop task (1)
static constexpr uint32_t MIN_DRDY_TIMEOUT_MS = 20;
static constexpr uint32_t RESET_LOW_MS = 10;  // > 2048 CLKIN periods at the 0.3 MHz minimum clock (6.8 ms)
static constexpr uint32_t STARTUP_DELAY_MS = 10;
static constexpr uint32_t RETRY_DELAY_MS = 1000;
static constexpr uint32_t MAX_CONSECUTIVE_TIMEOUTS = 50;
static constexpr uint32_t MAX_CONSECUTIVE_CRC_ERRORS = 50;
static constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 1000;

// Indexed by ADS131M0xPowerMode; the last entry is the fallback for out-of-range values
PROGMEM_STRING_TABLE(PowerModeStrings, "very low power", "low power", "high resolution", "unknown");

void ADS131M0x::setup() {
  this->spi_setup();
  this->drdy_pin_->setup();
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  if (!this->task_.create(task_entry, "ads131m0x", TASK_STACK_SIZE, this, TASK_PRIORITY, false)) {
    this->mark_failed(LOG_STR("Failed to create sampling task"));
    return;
  }
  this->task_handle_ = this->task_.get_handle();
  this->drdy_pin_->attach_interrupt(&ADS131M0x::drdy_isr, this, gpio::INTERRUPT_FALLING_EDGE);

  this->last_health_check_ = millis();
  this->set_interval(HEALTH_CHECK_INTERVAL_MS, [this]() { this->check_health_(); });
}

void ADS131M0x::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ADS131M0x:\n"
                "  Channels: %u\n"
                "  Chip ID: 0x%04X\n"
                "  Power mode: %s\n"
                "  Data rate: %.1f SPS\n"
                "  Window: %" PRIu32 " ms",
                this->num_channels_, this->chip_id_.load(),
                LOG_STR_ARG(PowerModeStrings::get_log_str(this->power_mode_)), this->data_rate_, this->window_ms_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DRDY Pin: ", this->drdy_pin_);
  LOG_PIN("  RESET Pin: ", this->reset_pin_);
  for (uint8_t ch = 0; ch < this->num_channels_; ch++) {
    ESP_LOGCONFIG(TAG, "  CH%u: gain %u (+/-%.4f V)", ch, 1u << this->gain_codes_[ch], this->get_full_scale(ch));
  }
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

float ADS131M0x::get_full_scale(uint8_t channel) const {
  return INTERNAL_REFERENCE_V / float(1u << this->gain_codes_[channel]);
}

Snapshot ADS131M0x::get_snapshot() {
  taskENTER_CRITICAL(&this->lock_);
  Snapshot copy = this->snapshot_;
  taskEXIT_CRITICAL(&this->lock_);
  return copy;
}

void ADS131M0x::check_health_() {
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t elapsed = now - this->last_health_check_;
  this->last_health_check_ = now;
  if (elapsed == 0) {
    return;
  }

  const uint32_t frames = this->frames_.load();
  const uint32_t crc_errors = this->crc_errors_.load();
  const uint32_t device_resets = this->device_resets_.load();
  // Measure against real elapsed time: the first check can include slow setup of other components.
  const float rate = float(frames - this->last_frames_) * 1000.0f / float(elapsed);
  const uint32_t new_crc_errors = crc_errors - this->last_crc_errors_;
  const uint32_t new_resets = device_resets - this->last_device_resets_;
  this->last_frames_ = frames;
  this->last_crc_errors_ = crc_errors;
  this->last_device_resets_ = device_resets;

  if (new_crc_errors > 0) {
    ESP_LOGW(TAG, "%" PRIu32 " frames dropped due to CRC errors (total %" PRIu32 ")", new_crc_errors, crc_errors);
  }
  if (new_resets > 0) {
    ESP_LOGW(TAG, "Unexpected device reset detected, reconfiguring (total %" PRIu32 ")", device_resets);
  }

  // Tolerate short overload, but a quarter of the nominal rate means sampling is broken.
  const bool healthy = this->configured_.load() && rate >= this->data_rate_ / 4.0f;
  if (healthy == this->healthy_.load()) {
    return;
  }
  this->healthy_.store(healthy);
  if (healthy) {
    ESP_LOGI(TAG, "Sampling at %.0f SPS", rate);
    this->status_clear_warning();
  } else {
    ESP_LOGW(TAG, "No data from ADC (%.0f SPS)", rate);
    this->status_set_warning(LOG_STR("No data from ADC"));
  }
}

void IRAM_ATTR ADS131M0x::drdy_isr(ADS131M0x *self) {
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(self->task_handle_, &woken);
  portYIELD_FROM_ISR(woken);
}

void ADS131M0x::task_entry(void *arg) { static_cast<ADS131M0x *>(arg)->task_loop_(); }

size_t ADS131M0x::frame_bytes_() const { return (2 + this->num_channels_) * WORD_BYTES; }

bool ADS131M0x::transfer_frame_(uint16_t command, uint16_t data, uint16_t *response, int32_t *samples) {
  std::array<uint8_t, MAX_FRAME_BYTES> buf{};
  const size_t len = this->frame_bytes_();
  buf[0] = command >> 8;
  buf[1] = command & 0xFF;
  buf[3] = data >> 8;
  buf[4] = data & 0xFF;

  this->enable();
  this->transfer_array(buf.data(), len);
  this->disable();

  // The output CRC covers every word before the CRC word, including the padding byte of each word.
  const size_t crc_pos = len - WORD_BYTES;
  const uint16_t expected = (uint16_t(buf[crc_pos]) << 8) | buf[crc_pos + 1];
  if (crc16be(buf.data(), crc_pos, CRC_INIT, CRC_CCITT_POLY) != expected) {
    return false;
  }

  if (response != nullptr) {
    *response = (uint16_t(buf[0]) << 8) | buf[1];
  }
  if (samples != nullptr) {
    for (uint8_t ch = 0; ch < this->num_channels_; ch++) {
      const uint8_t *word = &buf[(1 + ch) * WORD_BYTES];
      int32_t value = (int32_t(word[0]) << 16) | (int32_t(word[1]) << 8) | word[2];
      if (value & 0x800000) {
        value -= 0x1000000;
      }
      samples[ch] = value;
    }
  }
  return true;
}

bool ADS131M0x::read_register_(uint8_t address, uint16_t *value) {
  // The response to a command arrives in the following frame.
  return this->transfer_frame_(CMD_RREG | (uint16_t(address) << REG_ADDRESS_SHIFT), 0, nullptr, nullptr) &&
         this->transfer_frame_(CMD_NULL, 0, value, nullptr);
}

bool ADS131M0x::write_register_(uint8_t address, uint16_t value) {
  if (!this->transfer_frame_(CMD_WREG | (uint16_t(address) << REG_ADDRESS_SHIFT), value, nullptr, nullptr) ||
      !this->transfer_frame_(CMD_NULL, 0, nullptr, nullptr)) {
    ESP_LOGW(TAG, "Register 0x%02X: write failed (CRC)", address);
    return false;
  }
  uint16_t readback = 0;
  if (!this->read_register_(address, &readback)) {
    ESP_LOGW(TAG, "Register 0x%02X: readback failed (CRC)", address);
    return false;
  }
  if (readback != value) {
    ESP_LOGW(TAG, "Register 0x%02X: wrote 0x%04X, read back 0x%04X", address, value, readback);
    return false;
  }
  return true;
}

bool ADS131M0x::configure_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    vTaskDelay(pdMS_TO_TICKS(RESET_LOW_MS));
    this->reset_pin_->digital_write(true);
  }
  vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));
  // Discard whatever was pending; its CRC may be invalid after a reset.
  this->transfer_frame_(CMD_NULL, 0, nullptr, nullptr);

  uint16_t id = 0;
  if (!this->read_register_(REG_ID, &id)) {
    ESP_LOGW(TAG, "No valid response from device (CRC)");
    return false;
  }
  this->chip_id_.store(id);
  if ((id >> 12) != ID_FAMILY || ((id >> 8) & 0x0F) != this->num_channels_) {
    ESP_LOGW(TAG, "Unexpected chip ID 0x%04X for a %u-channel model", id, this->num_channels_);
    return false;
  }

  const uint16_t channel_mask = (1u << this->num_channels_) - 1;
  const uint16_t clock = (channel_mask << CLOCK_CH_EN_SHIFT) | (uint16_t(this->osr_code_ & 0x07) << CLOCK_OSR_SHIFT) |
                         (this->power_mode_ & 0x03);
  uint16_t gain1 = 0;
  uint16_t gain2 = 0;
  for (uint8_t ch = 0; ch < this->num_channels_; ch++) {
    const uint16_t field = uint16_t(this->gain_codes_[ch] & 0x07) << (4 * (ch % GAIN_CHANNELS_PER_REGISTER));
    if (ch < GAIN_CHANNELS_PER_REGISTER) {
      gain1 |= field;
    } else {
      gain2 |= field;
    }
  }

  bool ok = this->write_register_(REG_MODE, MODE_VALUE);
  ok = ok && this->write_register_(REG_CLOCK, clock);
  ok = ok && this->write_register_(REG_GAIN1, gain1);
  if (this->num_channels_ > GAIN_CHANNELS_PER_REGISTER) {
    ok = ok && this->write_register_(REG_GAIN2, gain2);
  }
  if (ok) {
    ESP_LOGD(TAG, "Configured (chip ID 0x%04X)", id);
  }
  return ok;
}

void ADS131M0x::task_loop_() {
  const uint32_t window_samples =
      std::max<uint32_t>(1, uint32_t(std::lround(this->data_rate_ * float(this->window_ms_) / 1000.0f)));
  const uint32_t drdy_timeout_ms =
      std::max<uint32_t>(MIN_DRDY_TIMEOUT_MS, uint32_t(std::ceil(4000.0f / this->data_rate_)));
  std::array<int64_t, MAX_CHANNELS> sum{};
  std::array<int64_t, MAX_CHANNELS> sum_sq{};
  std::array<int32_t, MAX_CHANNELS> samples{};
  uint32_t count = 0;
  uint32_t consecutive_timeouts = 0;
  uint32_t consecutive_crc_errors = 0;
  bool configured = false;

  while (true) {
    if (!configured) {
      this->configured_.store(false);
      configured = this->configure_();
      if (!configured) {
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        continue;
      }
      this->configured_.store(true);
      count = 0;
      sum.fill(0);
      sum_sq.fill(0);
      consecutive_timeouts = 0;
      consecutive_crc_errors = 0;
      ulTaskNotifyTake(pdTRUE, 0);
      continue;
    }

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(drdy_timeout_ms)) == 0) {
      this->drdy_timeouts_++;
      // An unread conversion holds DRDY low, so no falling edge follows; reading a frame releases it.
      this->transfer_frame_(CMD_NULL, 0, nullptr, nullptr);
      if (++consecutive_timeouts >= MAX_CONSECUTIVE_TIMEOUTS) {
        ESP_LOGW(TAG, "DRDY not toggling, reconfiguring");
        configured = false;
      }
      continue;
    }
    consecutive_timeouts = 0;

    uint16_t status = 0;
    if (!this->transfer_frame_(CMD_NULL, 0, &status, samples.data())) {
      this->crc_errors_++;
      if (++consecutive_crc_errors >= MAX_CONSECUTIVE_CRC_ERRORS) {
        configured = false;
      }
      continue;
    }
    consecutive_crc_errors = 0;

    if (status & STATUS_RESET) {
      // Registers are back at their defaults (gain 1, default data rate): this window is invalid.
      this->device_resets_++;
      configured = false;
      continue;
    }

    this->frames_++;
    for (uint8_t ch = 0; ch < this->num_channels_; ch++) {
      sum[ch] += samples[ch];
      sum_sq[ch] += int64_t(samples[ch]) * samples[ch];
    }
    if (++count < window_samples) {
      continue;
    }

    Snapshot result;
    for (uint8_t ch = 0; ch < this->num_channels_; ch++) {
      const float volts_per_code = this->get_full_scale(ch) / CODE_FULL_SCALE;
      const double mean = double(sum[ch]) / count;
      // AC RMS = sqrt(E[x^2] - E[x]^2): removes the DC offset of the input
      const double variance = double(sum_sq[ch]) / count - mean * mean;
      result.mean[ch] = float(mean) * volts_per_code;
      result.rms[ch] = variance > 0 ? float(std::sqrt(variance)) * volts_per_code : 0.0f;
    }
    sum.fill(0);
    sum_sq.fill(0);
    count = 0;
    result.timestamp_ms = millis();

    taskENTER_CRITICAL(&this->lock_);
    result.sequence = this->snapshot_.sequence + 1;
    if (result.sequence == 0) {
      result.sequence = 1;  // 0 means "no data yet"
    }
    this->snapshot_ = result;
    taskEXIT_CRITICAL(&this->lock_);
  }
}

}  // namespace esphome::ads131m0x

#endif  // USE_ESP32

#include "ads131m0x.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cmath>
#include <cstring>

#include <esp_heap_caps.h>
#include <sdkconfig.h>
#include <soc/soc_caps.h>

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
// Conversions the chip buffers per channel (datasheet 8.5.1.9.1)
static constexpr uint32_t FIFO_DEPTH = 2;
#if defined(SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE) && SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
// Internal RAM is cached (ESP32-P4): the SPI DMA needs a cache-line aligned receive address and length, or the
// driver allocates a bounce buffer for every transfer. The chip accepts frames longer than needed.
static constexpr size_t DMA_ALIGNMENT = CONFIG_CACHE_L1_CACHE_LINE_SIZE;
static constexpr bool DMA_PAD_LENGTH = true;
#else
// Word-aligned address only; a stack buffer could make the driver allocate a bounce buffer per transfer
static constexpr size_t DMA_ALIGNMENT = 4;
static constexpr bool DMA_PAD_LENGTH = false;
#endif
static constexpr float CODE_FULL_SCALE = 8388608.0f;  // 2^23
static constexpr float INTERNAL_REFERENCE_V = 1.2f;
static constexpr uint16_t CRC_INIT = 0xFFFF;
static constexpr uint16_t CRC_CCITT_POLY = 0x1021;

static constexpr uint32_t TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 5;  // above the main loop task (1)
#if CONFIG_FREERTOS_UNICORE
static constexpr BaseType_t TASK_CORE = tskNO_AFFINITY;
#else
// The network stack runs on core 0 at priorities up to 18 and would delay the task past the two-sample FIFO.
static constexpr BaseType_t TASK_CORE = 1;
#endif
static constexpr uint32_t MIN_DRDY_TIMEOUT_MS = 20;
static constexpr uint32_t RESET_LOW_MS = 10;  // > 2048 CLKIN periods at the 0.3 MHz minimum clock (6.8 ms)
static constexpr uint32_t STARTUP_DELAY_MS = 10;
static constexpr uint32_t RETRY_DELAY_MS = 1000;
static constexpr uint32_t MAX_CONSECUTIVE_TIMEOUTS = 50;
static constexpr uint32_t MAX_CONSECUTIVE_CRC_ERRORS = 50;
static constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 1000;
// The sample rate is averaged over this many health checks before it is compared with the nominal rate, so a
// single blocked loop iteration does not skew it.
static constexpr uint8_t RATE_AVERAGE_CHECKS = 10;
// Below this fraction of the nominal rate, conversions are being lost (e.g. too many fast devices on one bus).
static constexpr float RATE_DEGRADED_FRACTION = 0.95f;

// Indexed by ADS131M0xPowerMode; the last entry is the fallback for out-of-range values
PROGMEM_STRING_TABLE(PowerModeStrings, "very low power", "low power", "high resolution", "unknown");

void ADS131M0x::setup() {
  const size_t frame_bytes = this->frame_bytes_();
  this->transfer_bytes_ =
      DMA_PAD_LENGTH ? (frame_bytes + DMA_ALIGNMENT - 1) / DMA_ALIGNMENT * DMA_ALIGNMENT : frame_bytes;
  this->frame_buf_ = static_cast<uint8_t *>(
      heap_caps_aligned_calloc(DMA_ALIGNMENT, 1, this->transfer_bytes_, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (this->frame_buf_ == nullptr) {
    this->mark_failed(LOG_STR("Failed to allocate the frame buffer"));
    return;
  }

  this->spi_setup();
  this->drdy_pin_->setup();
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }

  if (!this->task_.create(task_entry, "ads131m0x", TASK_STACK_SIZE, this, TASK_PRIORITY, false, TASK_CORE)) {
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
                "  Sample rate: %.1f SPS\n"
                "  Window: %" PRIu32 " ms",
                this->num_channels_, this->chip_id_.load(),
                LOG_STR_ARG(PowerModeStrings::get_log_str(this->power_mode_)), this->sample_rate_, this->window_ms_);
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
  const bool healthy = this->configured_.load() && rate >= this->sample_rate_ / 4.0f;
  const bool was_healthy = this->healthy_.exchange(healthy);
  this->check_rate_(healthy ? elapsed : 0, frames);
  if (healthy && !was_healthy) {
    ESP_LOGI(TAG, "Sampling at %.0f SPS", rate);
    this->status_clear_warning();
  } else if (!healthy && (was_healthy || !this->status_has_warning())) {
    // Also covers a device that never delivered data since boot
    ESP_LOGW(TAG, "No data from ADC (%.0f SPS)", rate);
    this->status_set_warning(LOG_STR("No data from ADC"));
  }
}

void ADS131M0x::check_rate_(uint32_t elapsed_ms, uint32_t frames) {
  if (elapsed_ms == 0) {
    // Not sampling: start the average over once sampling resumes
    this->rate_checks_ = 0;
    return;
  }
  // The first check only sets the baseline: its elapsed time belongs to frames counted before it.
  if (this->rate_checks_ > 0) {
    this->rate_elapsed_ms_ += elapsed_ms;
  }
  if (this->rate_checks_ >= RATE_AVERAGE_CHECKS) {
    const float rate = float(frames - this->rate_start_frames_) * 1000.0f / float(this->rate_elapsed_ms_);
    ESP_LOGV(TAG, "Sampling at %.1f SPS (average over %" PRIu32 " ms)", rate, this->rate_elapsed_ms_);
    const bool degraded = rate < this->sample_rate_ * RATE_DEGRADED_FRACTION;
    if (degraded && !this->rate_degraded_) {
      ESP_LOGW(TAG, "Sampling at %.0f of %.0f SPS: conversions are being lost", rate, this->sample_rate_);
    } else if (!degraded && this->rate_degraded_) {
      ESP_LOGI(TAG, "Sampling at %.0f SPS again", rate);
    }
    this->rate_degraded_ = degraded;
    this->rate_checks_ = 0;
  }
  if (this->rate_checks_ == 0) {
    this->rate_start_frames_ = frames;
    this->rate_elapsed_ms_ = 0;
  }
  this->rate_checks_++;
}

void IRAM_ATTR ADS131M0x::drdy_isr(ADS131M0x *self) {
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(self->task_handle_, &woken);
  portYIELD_FROM_ISR(woken);
}

void ADS131M0x::task_entry(void *arg) { static_cast<ADS131M0x *>(arg)->task_loop_(); }

size_t ADS131M0x::frame_bytes_() const { return (2 + this->num_channels_) * WORD_BYTES; }

bool ADS131M0x::transfer_frame_(uint16_t command, uint16_t data, uint16_t *response, int32_t *samples) {
  uint8_t *buf = this->frame_buf_;
  const size_t len = this->frame_bytes_();
  std::memset(buf, 0, this->transfer_bytes_);
  buf[0] = command >> 8;
  buf[1] = command & 0xFF;
  buf[3] = data >> 8;
  buf[4] = data & 0xFF;

  this->enable();
  this->transfer_array(buf, this->transfer_bytes_);
  this->disable();

  // The output CRC covers every word before the CRC word, including the padding byte of each word.
  const size_t crc_pos = len - WORD_BYTES;
  const uint16_t expected = (uint16_t(buf[crc_pos]) << 8) | buf[crc_pos + 1];
  if (crc16be(buf, crc_pos, CRC_INIT, CRC_CCITT_POLY) != expected) {
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

void ADS131M0x::drain_fifo_() {
  for (uint32_t i = 0; i < FIFO_DEPTH; i++) {
    this->transfer_frame_(CMD_NULL, 0, nullptr, nullptr);
  }
}

void ADS131M0x::invalidate_snapshot_() {
  taskENTER_CRITICAL(&this->lock_);
  this->snapshot_ = Snapshot{};
  taskEXIT_CRITICAL(&this->lock_);
}

bool ADS131M0x::read_register_(uint8_t address, uint16_t *value) {
  // The response to a command arrives in the following frame.
  return this->transfer_frame_(CMD_RREG | (uint16_t(address) << REG_ADDRESS_SHIFT), 0, nullptr, nullptr) &&
         this->transfer_frame_(CMD_NULL, 0, value, nullptr);
}

bool ADS131M0x::write_register_(uint8_t address, uint16_t value) {
  if (!this->transfer_frame_(CMD_WREG | (uint16_t(address) << REG_ADDRESS_SHIFT), value, nullptr, nullptr) ||
      !this->transfer_frame_(CMD_NULL, 0, nullptr, nullptr)) {
    ESP_LOGD(TAG, "Register 0x%02X: write failed (CRC)", address);
    return false;
  }
  uint16_t readback = 0;
  if (!this->read_register_(address, &readback)) {
    ESP_LOGD(TAG, "Register 0x%02X: readback failed (CRC)", address);
    return false;
  }
  if (readback != value) {
    ESP_LOGD(TAG, "Register 0x%02X: wrote 0x%04X, read back 0x%04X", address, value, readback);
    return false;
  }
  return true;
}

const LogString *ADS131M0x::configure_() {
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
    return LOG_STR("no valid response from device (CRC)");
  }
  this->chip_id_.store(id);
  if ((id >> 12) != ID_FAMILY || ((id >> 8) & 0x0F) != this->num_channels_) {
    ESP_LOGD(TAG, "Chip ID 0x%04X, expected a %u-channel model", id, this->num_channels_);
    return LOG_STR("unexpected chip ID");
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
  if (!ok) {
    return LOG_STR("register write not verified by readback");
  }
  ESP_LOGD(TAG, "Configured (chip ID 0x%04X)", id);
  return nullptr;
}

void ADS131M0x::task_loop_() {
  const uint32_t window_samples =
      std::max<uint32_t>(1, uint32_t(std::lround(this->sample_rate_ * float(this->window_ms_) / 1000.0f)));
  const uint32_t drdy_timeout_ms =
      std::max<uint32_t>(MIN_DRDY_TIMEOUT_MS, uint32_t(std::ceil(4000.0f / this->sample_rate_)));
  std::array<int64_t, MAX_CHANNELS> sum{};
  // A square is never negative; unsigned keeps a full window of -2^23 codes (2^63) representable
  std::array<uint64_t, MAX_CHANNELS> sum_sq{};
  std::array<int32_t, MAX_CHANNELS> samples{};
  uint32_t count = 0;
  uint32_t consecutive_timeouts = 0;
  uint32_t consecutive_crc_errors = 0;
  uint32_t failed_attempts = 0;
  bool configured = false;

  // A window only holds consecutive conversions; after a gap it starts over.
  auto restart_window = [&]() {
    count = 0;
    sum.fill(0);
    sum_sq.fill(0);
  };

  while (true) {
    if (!configured) {
      // Readings from before the reconfiguration must not be published as current
      this->invalidate_snapshot_();
      this->configured_.store(false);
      const LogString *error = this->configure_();
      if (error != nullptr) {
        // Warn once; a missing or miswired chip would otherwise log a warning every retry
        if (failed_attempts++ == 0) {
          ESP_LOGW(TAG, "Configuration failed: %s; retrying every %" PRIu32 " ms", LOG_STR_ARG(error), RETRY_DELAY_MS);
        } else {
          ESP_LOGD(TAG, "Configuration failed: %s", LOG_STR_ARG(error));
        }
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        continue;
      }
      if (failed_attempts > 0) {
        ESP_LOGI(TAG, "Configured after %" PRIu32 " failed attempts", failed_attempts);
        failed_attempts = 0;
      }
      configured = true;
      this->configured_.store(true);
      restart_window();
      consecutive_timeouts = 0;
      consecutive_crc_errors = 0;
      // Conversions ran unread during configuration: empty the FIFO, then follow DRDY from the next one
      this->drain_fifo_();
      ulTaskNotifyTake(pdTRUE, 0);
      continue;
    }

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(drdy_timeout_ms)) == 0) {
      this->drdy_timeouts_++;
      // A full FIFO holds DRDY low, so no falling edge follows; reading both slots releases it.
      this->drain_fifo_();
      restart_window();
      if (++consecutive_timeouts >= MAX_CONSECUTIVE_TIMEOUTS) {
        ESP_LOGW(TAG, "DRDY not toggling, reconfiguring");
        configured = false;
      }
      continue;
    }
    consecutive_timeouts = 0;

    // DRDY stays low while a conversion is buffered. Follow its level rather than counting edges: reading the
    // first of two buffered conversions makes a short DRDY pulse, which would count as an extra edge.
    for (uint32_t i = 0; i < FIFO_DEPTH && configured && !this->drdy_pin_->digital_read(); i++) {
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
        sum_sq[ch] += uint64_t(int64_t(samples[ch]) * samples[ch]);
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
      restart_window();
      result.timestamp_ms = millis();
      if (++this->window_sequence_ == 0) {
        this->window_sequence_ = 1;  // 0 means "no valid data"
      }
      result.sequence = this->window_sequence_;

      taskENTER_CRITICAL(&this->lock_);
      this->snapshot_ = result;
      taskEXIT_CRITICAL(&this->lock_);
    }
  }
}

}  // namespace esphome::ads131m0x

#endif  // USE_ESP32

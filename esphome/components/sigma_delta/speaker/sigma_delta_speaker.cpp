#ifdef USE_ESP32

#include "sigma_delta_speaker.h"
#include "esphome/core/log.h"

#include <cstring>
#include <algorithm>
#include "esp_heap_caps.h"

static const char *const TAG = "speaker.sigma_delta";

namespace esphome::sigma_delta {

void SigmaDeltaSpeaker::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Sigma Delta speaker...");

  this->ring_buf_ = (int8_t *) heap_caps_malloc(RING_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!this->ring_buf_) {
    // Fallback to any caps is unsafe for ISR (must be internal); fail instead of using SPIRAM.
    ESP_LOGE(TAG, "Failed to allocate ring buffer (%u bytes, internal)", (unsigned) RING_SIZE);
    this->mark_failed();
    return;
  }
  memset(this->ring_buf_, 0, RING_SIZE);
  this->ring_read_ = 0;
  this->ring_write_ = 0;

  sdm_config_t sdm_cfg = {};
  sdm_cfg.clk_src = SDM_CLK_SRC_DEFAULT;
  sdm_cfg.gpio_num = this->pin_->get_pin();
  sdm_cfg.sample_rate_hz = this->oversample_rate_;

  esp_err_t err = sdm_new_channel(&sdm_cfg, &this->sdm_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "sdm_new_channel failed: %s", esp_err_to_name(err));
    heap_caps_free(this->ring_buf_);
    this->ring_buf_ = nullptr;
    this->mark_failed();
    return;
  }

  err = sdm_channel_enable(this->sdm_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "sdm_channel_enable failed: %s", esp_err_to_name(err));
    sdm_del_channel(this->sdm_handle_);
    this->sdm_handle_ = nullptr;
    heap_caps_free(this->ring_buf_);
    this->ring_buf_ = nullptr;
    this->mark_failed();
    return;
  }

  gptimer_config_t timer_cfg = {};
  timer_cfg.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  timer_cfg.direction = GPTIMER_COUNT_UP;
  timer_cfg.resolution_hz = 1'000'000;

  err = gptimer_new_timer(&timer_cfg, &this->timer_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gptimer_new_timer failed: %s", esp_err_to_name(err));
    sdm_channel_disable(this->sdm_handle_);
    sdm_del_channel(this->sdm_handle_);
    this->sdm_handle_ = nullptr;
    heap_caps_free(this->ring_buf_);
    this->ring_buf_ = nullptr;
    this->mark_failed();
    return;
  }

  gptimer_alarm_config_t alarm_cfg = {};
  alarm_cfg.alarm_count = 1'000'000 / this->sample_rate_;
  alarm_cfg.reload_count = 0;
  alarm_cfg.flags.auto_reload_on_alarm = true;

  err = gptimer_set_alarm_action(this->timer_handle_, &alarm_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gptimer_set_alarm_action failed: %s", esp_err_to_name(err));
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
    sdm_channel_disable(this->sdm_handle_);
    sdm_del_channel(this->sdm_handle_);
    this->sdm_handle_ = nullptr;
    heap_caps_free(this->ring_buf_);
    this->ring_buf_ = nullptr;
    this->mark_failed();
    return;
  }

  // Register ISR callback once during setup (not on every start()).
  gptimer_event_callbacks_t cbs = {};
  cbs.on_alarm = &SigmaDeltaSpeaker::timer_isr;
  err = gptimer_register_event_callbacks(this->timer_handle_, &cbs, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gptimer_register_event_callbacks failed: %s", esp_err_to_name(err));
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
    sdm_channel_disable(this->sdm_handle_);
    sdm_del_channel(this->sdm_handle_);
    this->sdm_handle_ = nullptr;
    heap_caps_free(this->ring_buf_);
    this->ring_buf_ = nullptr;
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Sigma Delta speaker configured: pin=%d sample_rate=%lu oversample=%lu ring=%u", this->pin_->get_pin(),
           (unsigned long) this->sample_rate_, (unsigned long) this->oversample_rate_, (unsigned) RING_SIZE);
}

void SigmaDeltaSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG, "Sigma Delta Speaker:");
  if (this->pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Pin: %d", this->pin_->get_pin());
  }
  ESP_LOGCONFIG(TAG, "  Sample rate: %lu Hz", (unsigned long) this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  Oversample rate: %lu Hz", (unsigned long) this->oversample_rate_);
  ESP_LOGCONFIG(TAG, "  Bits per sample: %u", this->bits_per_sample_);
  ESP_LOGCONFIG(TAG, "  Channels: %u", this->num_channels_);
}

void SigmaDeltaSpeaker::loop() {
  // No pending free handling needed for ring buffer
}

void SigmaDeltaSpeaker::start() {
  if (this->running_)
    return;
  if (!this->sdm_handle_ || !this->timer_handle_) {
    ESP_LOGE(TAG, "start() called but handles not initialized (setup failed?)");
    return;
  }

  ESP_LOGI(TAG, "Starting Sigma Delta playback");

  esp_err_t err = gptimer_enable(this->timer_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gptimer_enable failed: %s", esp_err_to_name(err));
    return;
  }

  err = gptimer_start(this->timer_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "gptimer_start failed: %s", esp_err_to_name(err));
    gptimer_disable(this->timer_handle_);
    return;
  }

  this->running_ = true;
  this->state_ = speaker::STATE_RUNNING;
}

void SigmaDeltaSpeaker::stop() {
  if (!this->running_)
    return;

  ESP_LOGI(TAG, "Stopping Sigma Delta playback");

  if (this->timer_handle_) {
    esp_err_t err = gptimer_stop(this->timer_handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "gptimer_stop failed: %s", esp_err_to_name(err));
    }
    err = gptimer_disable(this->timer_handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "gptimer_disable failed: %s", esp_err_to_name(err));
    }
  }
  if (this->sdm_handle_) {
    esp_err_t err = sdm_channel_set_pulse_density(this->sdm_handle_, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "sdm_channel_set_pulse_density(0) failed: %s", esp_err_to_name(err));
    }
  }

  this->ring_read_ = 0;
  this->ring_write_ = 0;

  this->running_ = false;
  this->state_ = speaker::STATE_STOPPED;
}

size_t SigmaDeltaSpeaker::play(const uint8_t *data, size_t length) {
  if (!this->sdm_handle_ || !this->timer_handle_ || !this->ring_buf_) {
    ESP_LOGE(TAG, "play() called but not initialized");
    return 0;
  }
  if (!this->running_) {
    this->start();
    if (!this->running_)
      return 0;
  }

  size_t frames;
  size_t samples_out;
  size_t bps = this->bits_per_sample_;
  size_t bytes_per_sample = (bps + 7) / 8;

  if (this->num_channels_ == 2) {
    frames = length / (bytes_per_sample * 2);
    samples_out = frames;
  } else {
    frames = length / bytes_per_sample;
    samples_out = frames;
  }

  // Check free space — branch instead of modulo (ISR-unsafe division avoidance)
  size_t r = this->ring_read_;
  size_t w = this->ring_write_;
  size_t avail = (w >= r) ? (RING_SIZE - (w - r) - 1) : (r - w - 1);
  if (samples_out > avail) {
    size_t need = samples_out - avail;
    size_t nr = r + need;
    if (nr >= RING_SIZE)
      nr -= RING_SIZE;
    this->ring_read_ = nr;
  }

  // Helper to advance write pointer with branch (no modulo division)
  auto advance_write = [this]() {
    size_t nw = this->ring_write_ + 1;
    if (nw >= RING_SIZE)
      nw = 0;
    this->ring_write_ = nw;
  };

  if (this->num_channels_ == 2) {
    if (bps == 16) {
      const int16_t *src = reinterpret_cast<const int16_t *>(data);
      for (size_t i = 0; i < frames; i++) {
        int32_t mixed = (static_cast<int32_t>(src[i * 2]) + static_cast<int32_t>(src[i * 2 + 1])) / 2;
        int32_t v = mixed >> 8;
        v = std::clamp<int32_t>(v, -128, 127);
        this->ring_buf_[this->ring_write_] = static_cast<int8_t>(v);
        advance_write();
      }
    } else if (bps == 8) {
      const int8_t *src = reinterpret_cast<const int8_t *>(data);
      for (size_t i = 0; i < frames; i++) {
        int32_t mixed = (static_cast<int32_t>(src[i * 2]) + static_cast<int32_t>(src[i * 2 + 1])) / 2;
        mixed = std::clamp<int32_t>(mixed, -128, 127);
        this->ring_buf_[this->ring_write_] = static_cast<int8_t>(mixed);
        advance_write();
      }
    } else if (bps == 24) {
      // 24-bit little-endian packed: 3 bytes per sample, stereo interleaved
      for (size_t i = 0; i < frames; i++) {
        size_t base = i * 6;
        int32_t l = (static_cast<int32_t>(data[base]) | (static_cast<int32_t>(data[base + 1]) << 8) |
                     (static_cast<int32_t>(static_cast<int8_t>(data[base + 2])) << 16));
        int32_t rch = (static_cast<int32_t>(data[base + 3]) | (static_cast<int32_t>(data[base + 4]) << 8) |
                       (static_cast<int32_t>(static_cast<int8_t>(data[base + 5])) << 16));
        int32_t mixed = (l + rch) / 2;
        int32_t v = mixed >> 16;
        v = std::clamp<int32_t>(v, -128, 127);
        this->ring_buf_[this->ring_write_] = static_cast<int8_t>(v);
        advance_write();
      }
    } else if (bps == 32) {
      const int32_t *src = reinterpret_cast<const int32_t *>(data);
      for (size_t i = 0; i < frames; i++) {
        int32_t mixed = (src[i * 2] / 2) + (src[i * 2 + 1] / 2);
        int32_t v = mixed >> 24;
        v = std::clamp<int32_t>(v, -128, 127);
        this->ring_buf_[this->ring_write_] = static_cast<int8_t>(v);
        advance_write();
      }
    } else {
      ESP_LOGE(TAG, "Unsupported bits_per_sample %u", (unsigned) bps);
      return 0;
    }
  } else {  // mono
    if (bps == 16) {
      const int16_t *src = reinterpret_cast<const int16_t *>(data);
      for (size_t i = 0; i < frames; i++) {
        int32_t v = static_cast<int32_t>(src[i]) >> 8;
        v = std::clamp<int32_t>(v, -128, 127);
        this->ring_buf_[this->ring_write_] = static_cast<int8_t>(v);
        advance_write();
      }
    } else if (bps == 8) {
      for (size_t i = 0; i < frames; i++) {
        this->ring_buf_[this->ring_write_] = reinterpret_cast<const int8_t *>(data)[i];
        advance_write();
      }
    } else if (bps == 24) {
      for (size_t i = 0; i < frames; i++) {
        size_t base = i * 3;
        int32_t s = (static_cast<int32_t>(data[base]) | (static_cast<int32_t>(data[base + 1]) << 8) |
                     (static_cast<int32_t>(static_cast<int8_t>(data[base + 2])) << 16));
        int32_t v = s >> 16;
        v = std::clamp<int32_t>(v, -128, 127);
        this->ring_buf_[this->ring_write_] = static_cast<int8_t>(v);
        advance_write();
      }
    } else if (bps == 32) {
      const int32_t *src = reinterpret_cast<const int32_t *>(data);
      for (size_t i = 0; i < frames; i++) {
        int32_t v = src[i] >> 24;
        v = std::clamp<int32_t>(v, -128, 127);
        this->ring_buf_[this->ring_write_] = static_cast<int8_t>(v);
        advance_write();
      }
    } else {
      ESP_LOGE(TAG, "Unsupported bits_per_sample %u", (unsigned) bps);
      return 0;
    }
  }

  return length;
}

bool SigmaDeltaSpeaker::has_buffered_data() const { return this->ring_read_ != this->ring_write_; }

bool IRAM_ATTR SigmaDeltaSpeaker::timer_isr(gptimer_handle_t, const gptimer_alarm_event_data_t *, void *user_ctx) {
  auto *self = static_cast<SigmaDeltaSpeaker *>(user_ctx);
  if (!self || !self->sdm_handle_ || !self->ring_buf_) {
    return false;
  }

  int8_t sample = 0;
  size_t r = self->ring_read_;
  size_t w = self->ring_write_;
  if (r != w) {
    sample = self->ring_buf_[r];
    r++;
    if (r >= RING_SIZE)
      r = 0;
    self->ring_read_ = r;
  }

  // NOTE: sdm_channel_set_pulse_density() is IRAM-safe only when
  // CONFIG_SDM_CTRL_FUNC_IN_IRAM=y (ESP-IDF >=5.1, driver/sdm). With that
  // option the control path is placed in IRAM and safe to call from an
  // ISR. If IRAM placement is not guaranteed on your IDF version, replace
  // this direct call with a deferred pattern: ISR sets a volatile flag /
  // ring pop and a high-priority task does the density write.
  sdm_channel_set_pulse_density(self->sdm_handle_, sample);
  return false;
}

SigmaDeltaSpeaker::~SigmaDeltaSpeaker() {
  // Ensure hardware stopped before freeing; mirrors on_shutdown logic.
  if (this->timer_handle_) {
    gptimer_stop(this->timer_handle_);
    gptimer_disable(this->timer_handle_);
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
  }
  if (this->sdm_handle_) {
    sdm_channel_disable(this->sdm_handle_);
    sdm_del_channel(this->sdm_handle_);
    this->sdm_handle_ = nullptr;
  }
  if (this->ring_buf_) {
    heap_caps_free(this->ring_buf_);
    this->ring_buf_ = nullptr;
  }
}

void SigmaDeltaSpeaker::on_shutdown() {
  this->stop();
  if (this->timer_handle_) {
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
  }
  if (this->sdm_handle_) {
    sdm_channel_disable(this->sdm_handle_);
    sdm_del_channel(this->sdm_handle_);
    this->sdm_handle_ = nullptr;
  }
  if (this->ring_buf_) {
    heap_caps_free(this->ring_buf_);
    this->ring_buf_ = nullptr;
  }
}

}  // namespace esphome::sigma_delta

#endif  // USE_ESP32

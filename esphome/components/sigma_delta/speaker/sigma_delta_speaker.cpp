#ifdef USE_ESP32

#include "sigma_delta_speaker.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

#include <esp_timer.h>
#include <freertos/task.h>

namespace esphome::sigma_delta {

ESPHOME_LOG_TAG(TAG, "sigma_delta.speaker");

static constexpr uint32_t OVERSAMPLE_RATE_HZ = 1'000'000;
// High enough that 44.1 kHz rounds to within 0.1% of the requested rate
static constexpr uint32_t TIMER_RESOLUTION_HZ = 10'000'000;
// Stop the sample interrupt after the ring has been empty this long
static constexpr uint32_t IDLE_TIMEOUT_MS = 100;
// Volume 0 maps to this attenuation, matching the i2s_audio software volume curve
static constexpr float SOFTWARE_VOLUME_MIN_DB = -49.0f;
static constexpr int32_t Q15_UNITY = 1 << 15;

void SigmaDeltaSpeaker::setup() {
  sdm_config_t sdm_cfg = {};
  sdm_cfg.clk_src = SDM_CLK_SRC_DEFAULT;
  sdm_cfg.gpio_num = this->pin_->get_pin();
  sdm_cfg.sample_rate_hz = OVERSAMPLE_RATE_HZ;
  sdm_cfg.flags.invert_out = this->pin_->is_inverted();

  gptimer_config_t timer_cfg = {};
  timer_cfg.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  timer_cfg.direction = GPTIMER_COUNT_UP;
  timer_cfg.resolution_hz = TIMER_RESOLUTION_HZ;

  gptimer_event_callbacks_t cbs = {};
  cbs.on_alarm = &SigmaDeltaSpeaker::timer_isr;

  esp_err_t err = sdm_new_channel(&sdm_cfg, &this->sdm_handle_);
  if (err == ESP_OK)
    err = sdm_channel_enable(this->sdm_handle_);
  if (err == ESP_OK)
    err = gptimer_new_timer(&timer_cfg, &this->timer_handle_);
  if (err == ESP_OK)
    err = gptimer_register_event_callbacks(this->timer_handle_, &cbs, this);
  if (err == ESP_OK)
    err = gptimer_enable(this->timer_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Hardware init failed: %s", esp_err_to_name(err));
    this->teardown_();
    this->mark_failed(LOG_STR("Sigma delta init failed"));
    return;
  }
}

void SigmaDeltaSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Sigma Delta Speaker:\n"
                "  Oversample rate: %" PRIu32 " Hz\n"
                "  Buffer: %u samples",
                OVERSAMPLE_RATE_HZ, (unsigned) RING_SIZE);
  LOG_PIN("  Pin: ", this->pin_);
}

void SigmaDeltaSpeaker::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  this->apply_command_();
  this->report_played_();
  if (this->state_ == speaker::STATE_STOPPING && !this->has_buffered_data()) {
    this->disarm_timer_();
    this->state_ = speaker::STATE_STOPPED;
  }
  const bool active = !this->is_stopped();
  if (active && this->has_buffered_data()) {
    this->last_data_ms_ = now;
    if (!this->timer_armed_.load(std::memory_order_relaxed))
      this->arm_timer_();
    return;
  }
  if (active && this->timer_armed_.load(std::memory_order_relaxed) && now - this->last_data_ms_ < IDLE_TIMEOUT_MS)
    return;

  this->disarm_timer_();
  this->report_played_();
  if (!active) {
    // A pending command means play() is restarting the speaker; its frames follow the request,
    // so leave them for the next pass instead of dropping them with the leftovers.
    const uint32_t write = this->ring_write_.load(std::memory_order_acquire);
    if (this->command_.load(std::memory_order_acquire) != COMMAND_NONE)
      return;
    // The ISR is stopped, so dropping what stop() left behind cannot race it. Dropped frames were never played.
    this->ring_read_.store(write, std::memory_order_release);
    this->last_reported_read_ = write;
    this->disable_loop();
    return;
  }
  // Pairs with the fence in play(): either play() sees the timer disarmed and wakes the loop, or we see its data
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if (!this->has_buffered_data())
    this->disable_loop();
}

void SigmaDeltaSpeaker::report_played_() {
  // One ring slot is one frame, since channels are mixed before the write
  const uint32_t read = this->ring_read_.load(std::memory_order_acquire);
  if (read != this->last_reported_read_) {
    this->audio_output_callback_.call(read - this->last_reported_read_, esp_timer_get_time());
    this->last_reported_read_ = read;
  }
}

void SigmaDeltaSpeaker::apply_command_() {
  switch (this->command_.exchange(COMMAND_NONE, std::memory_order_acq_rel)) {
    case COMMAND_START:
      if (!this->is_running()) {
        ESP_LOGV(TAG, "Starting");
        this->state_ = speaker::STATE_RUNNING;
      }
      break;
    case COMMAND_STOP:
      if (!this->is_stopped()) {
        ESP_LOGV(TAG, "Stopping");
        this->state_ = speaker::STATE_STOPPED;
      }
      break;
    case COMMAND_FINISH:
      if (this->is_running()) {
        ESP_LOGV(TAG, "Finishing");
        this->state_ = speaker::STATE_STOPPING;
      }
      break;
    default:
      break;
  }
}

void SigmaDeltaSpeaker::start() {
  if (this->is_failed())
    return;
  this->command_.store(COMMAND_START, std::memory_order_release);
  this->enable_loop_soon_any_context();
}

void SigmaDeltaSpeaker::stop() {
  this->command_.store(COMMAND_STOP, std::memory_order_release);
  this->enable_loop_soon_any_context();
}

void SigmaDeltaSpeaker::finish() {
  this->command_.store(COMMAND_FINISH, std::memory_order_release);
  this->enable_loop_soon_any_context();
}

size_t SigmaDeltaSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_failed())
    return 0;
  // Request the start before writing: the stopped path in loop() drops the ring only while no
  // command is pending, so frames written after this request are never dropped as leftovers.
  if (!this->is_running())
    this->start();

  size_t written = this->write_frames_(data, length);
  const TickType_t start_ticks = xTaskGetTickCount();
  while (written == 0 && xTaskGetTickCount() - start_ticks < ticks_to_wait) {
    vTaskDelay(1);
    written = this->write_frames_(data, length);
  }
  if (written > 0) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (!this->timer_armed_.load(std::memory_order_relaxed))
      this->enable_loop_soon_any_context();
  }
  return written;
}

size_t SigmaDeltaSpeaker::write_frames_(const uint8_t *data, size_t length) {
  const audio::AudioStreamInfo &info = this->audio_stream_info_;
  const size_t bytes_per_sample = info.samples_to_bytes(1);
  const uint8_t channels = info.get_channels();
  // Stream limits cap channels at 2, so averaging is a shift of 0 or 1
  const uint8_t mix_shift = channels - 1;

  const uint32_t write = this->ring_write_.load(std::memory_order_relaxed);
  const uint32_t read = this->ring_read_.load(std::memory_order_acquire);
  const uint32_t frames = std::min<uint32_t>(info.bytes_to_frames(length), RING_SIZE - (write - read));
  const int64_t gain = this->gain_q15_.load(std::memory_order_relaxed);

  auto fill = [&](int8_t *dst, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
      int32_t mixed = 0;
      for (uint8_t ch = 0; ch < channels; ch++) {
        mixed += audio::unpack_audio_sample_to_q31(data, bytes_per_sample) >> mix_shift;
        data += bytes_per_sample;
      }
      // Q31 * Q15 gain (<= 1.0), then keep the top 8 bits
      dst[i] = static_cast<int8_t>((mixed * gain) >> (15 + 24));
    }
  };

  const uint32_t start = write & (RING_SIZE - 1);
  const uint32_t first = std::min<uint32_t>(frames, RING_SIZE - start);
  fill(this->ring_.data() + start, first);
  fill(this->ring_.data(), frames - first);

  this->ring_write_.store(write + frames, std::memory_order_release);
  return info.frames_to_bytes(frames);
}

bool SigmaDeltaSpeaker::has_buffered_data() const {
  return this->ring_read_.load(std::memory_order_acquire) != this->ring_write_.load(std::memory_order_acquire);
}

void SigmaDeltaSpeaker::set_volume(float volume) {
  speaker::Speaker::set_volume(volume);
  this->update_gain_();
}

void SigmaDeltaSpeaker::set_mute_state(bool mute_state) {
  speaker::Speaker::set_mute_state(mute_state);
  this->update_gain_();
}

void SigmaDeltaSpeaker::update_gain_() {
  int32_t gain = Q15_UNITY;
#ifdef USE_AUDIO_DAC
  if (this->has_audio_dac_()) {
    this->gain_q15_.store(gain, std::memory_order_relaxed);  // Hardware volume
    return;
  }
#endif
  if (this->is_silent_()) {
    gain = 0;
  } else if (this->volume_ < 1.0f) {
    const float db = remap<float, float>(this->volume_, 0.0f, 1.0f, SOFTWARE_VOLUME_MIN_DB, 0.0f);
    gain = static_cast<int32_t>(lroundf(powf(10.0f, db / 20.0f) * Q15_UNITY));
  }
  this->gain_q15_.store(gain, std::memory_order_relaxed);
}

void SigmaDeltaSpeaker::arm_timer_() {
  const uint32_t rate = this->audio_stream_info_.get_sample_rate();
  gptimer_alarm_config_t alarm_cfg = {};
  alarm_cfg.alarm_count = (TIMER_RESOLUTION_HZ + rate / 2) / rate;
  alarm_cfg.flags.auto_reload_on_alarm = true;

  esp_err_t err = gptimer_set_raw_count(this->timer_handle_, 0);
  if (err == ESP_OK)
    err = gptimer_set_alarm_action(this->timer_handle_, &alarm_cfg);
  if (err == ESP_OK)
    err = gptimer_start(this->timer_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Timer start failed: %s", esp_err_to_name(err));
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  ESP_LOGV(TAG, "Timer armed at %" PRIu32 " Hz", rate);
  this->timer_armed_.store(1, std::memory_order_relaxed);
}

void SigmaDeltaSpeaker::disarm_timer_() {
  if (!this->timer_armed_.load(std::memory_order_relaxed))
    return;
  gptimer_stop(this->timer_handle_);
  sdm_channel_set_pulse_density(this->sdm_handle_, 0);
  this->timer_armed_.store(0, std::memory_order_relaxed);
  ESP_LOGV(TAG, "Timer idle");
}

bool IRAM_ATTR SigmaDeltaSpeaker::timer_isr(gptimer_handle_t, const gptimer_alarm_event_data_t *, void *user_ctx) {
  auto *self = static_cast<SigmaDeltaSpeaker *>(user_ctx);
  const uint32_t read = self->ring_read_.load(std::memory_order_relaxed);
  int8_t density = 0;
  if (read != self->ring_write_.load(std::memory_order_acquire)) {
    density = self->ring_[read & (RING_SIZE - 1)];
    self->ring_read_.store(read + 1, std::memory_order_release);
  }
  sdm_channel_set_pulse_density(self->sdm_handle_, density);
  return false;
}

void SigmaDeltaSpeaker::on_shutdown() { this->teardown_(); }

void SigmaDeltaSpeaker::teardown_() {
  if (this->timer_handle_ != nullptr) {
    this->disarm_timer_();
    gptimer_disable(this->timer_handle_);
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
  }
  if (this->sdm_handle_ != nullptr) {
    sdm_channel_disable(this->sdm_handle_);
    sdm_del_channel(this->sdm_handle_);
    this->sdm_handle_ = nullptr;
  }
}

}  // namespace esphome::sigma_delta

#endif  // USE_ESP32

#include "router_speaker.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include "esp_timer.h"

#include <algorithm>

namespace esphome::router {

static const char *const TAG = "router.speaker";

static inline uint32_t atomic_subtract_clamped(std::atomic<uint32_t> &var, uint32_t amount) {
  uint32_t current = var.load(std::memory_order_acquire);
  uint32_t subtracted = 0;
  if (current > 0) {
    uint32_t new_value;
    do {
      subtracted = std::min(amount, current);
      new_value = current - subtracted;
    } while (!var.compare_exchange_weak(current, new_value, std::memory_order_release, std::memory_order_acquire));
  }
  return subtracted;
}

void Router::setup() {
  // Register a callback on every configured output. Each lambda captures its own
  // index and only forwards when that output is the active one. This is required
  // because CallbackManager has no remove() API.
  for (size_t i = 0; i < this->outputs_.size(); i++) {
    this->outputs_[i]->add_audio_output_callback([this, i](uint32_t frames, int64_t timestamp_us) {
      // Always suppress the draining previous output during a switch, even if it's
      // also the reselected active output (switching back to the bus holder).
      // loop() fires one synthetic credit for its in-flight frames instead.
      if (this->pending_start_prev_idx_.load(std::memory_order_relaxed) == static_cast<int8_t>(i)) {
        return;
      }
      if (this->active_output_idx_.load(std::memory_order_relaxed) != static_cast<int8_t>(i)) {
        return;
      }
      atomic_subtract_clamped(this->frames_in_pipeline_, frames);
      this->audio_output_callback_.call(frames, timestamp_us);
    });
  }
}

void Router::loop() {
  speaker::Speaker *active = this->get_active_output();

  // Mid-switch: the new output's start() is deferred until the previous output
  // fully releases shared hardware (e.g. a single i2s_audio bus driving two
  // speakers). Starting earlier produces "Parent bus is busy" retries. The
  // synthetic-credit callback is also deferred until prev is fully stopped, so
  // that once its task has drained no natural callbacks can race ours.
  const int8_t pending_prev_idx = this->pending_start_prev_idx_.load(std::memory_order_relaxed);
  if (pending_prev_idx >= 0) {
    speaker::Speaker *prev = this->outputs_[pending_prev_idx];
    if (prev->is_stopped()) {
      this->pending_start_prev_idx_.store(-1, std::memory_order_relaxed);

      // Credit any frames left in prev's ring buffer / DMA so producer frame
      // accounting (SpeakerSourceMediaPlayer pending_frames, sendspin/AEC
      // clocks) clears cleanly. The leftover audio is intentionally dropped and
      // the producer is told it played "now", giving a clean discontinuity that
      // keeps frame accounting consistent across the switch.
      const uint32_t in_flight = this->frames_in_pipeline_.exchange(0, std::memory_order_acq_rel);
      if (in_flight > 0) {
        this->audio_output_callback_.call(in_flight, esp_timer_get_time());
      }

      this->apply_cached_state_to_active_();
      this->state_ = speaker::STATE_STARTING;
      active->start();
    }
    return;
  }

  // Mirror the active output's running/stopped state into our own state_ so that
  // is_running() / is_stopped() stay accurate from the producer's perspective.
  // Also catch the active output self-stopping (e.g. i2s_audio silence timeout):
  // without this, our state_ would stay RUNNING forever and the next play() would
  // skip start(). The output retains its own volume/mute across a restart (and we
  // forward those live regardless), but stream info arrives via the non-virtual
  // set_audio_stream_info() and never reaches the output on its own; if the format
  // changed while stopped, only start()'s apply_cached_state_to_active_() pushes it
  // down before the output's play()-side auto-start locks in the stale format.
  if (active->is_stopped()) {
    this->state_ = speaker::STATE_STOPPED;
  } else if (this->state_ == speaker::STATE_STARTING && active->is_running()) {
    this->state_ = speaker::STATE_RUNNING;
  }
}

void Router::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Router Speaker:\n"
                "  Outputs: %u",
                static_cast<unsigned>(this->outputs_.size()));
}

size_t Router::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  speaker::Speaker *active = this->get_active_output();

  // Drop frames during a mid-switch until the old output releases shared hardware;
  // forwarding now would trigger the new output's play()-side auto-start while
  // the bus is still busy.
  if (this->pending_start_prev_idx_.load(std::memory_order_relaxed) >= 0) {
    vTaskDelay(ticks_to_wait);
    return 0;
  }

  // Producers (e.g. mixer) set stream info on us and then drive play() from a
  // task without ever calling our start(). i2s_audio's play() auto-starts the
  // underlying driver, so we must push our cached stream info to the active
  // output before that auto-start, or it locks to its default (16k mono).
  if (this->state_ == speaker::STATE_STOPPED) {
    this->start();
    vTaskDelay(ticks_to_wait);
    ticks_to_wait = 0;
  }

  size_t written = active->play(data, length, ticks_to_wait);
  if (written > 0) {
    const uint32_t frames = this->audio_stream_info_.bytes_to_frames(written);
    this->frames_in_pipeline_.fetch_add(frames, std::memory_order_release);
  }
  return written;
}

void Router::start() {
  this->frames_in_pipeline_.store(0, std::memory_order_release);
  this->apply_cached_state_to_active_();
  this->state_ = speaker::STATE_STARTING;
  this->get_active_output()->start();
}

void Router::stop() {
  // Cancel any pending mid-switch start; the producer wants us stopped.
  this->pending_start_prev_idx_.store(-1, std::memory_order_relaxed);
  this->state_ = speaker::STATE_STOPPING;
  this->get_active_output()->stop();
}

void Router::finish() {
  this->pending_start_prev_idx_.store(-1, std::memory_order_relaxed);
  this->state_ = speaker::STATE_STOPPING;
  this->get_active_output()->finish();
}

bool Router::has_buffered_data() const { return this->get_active_output()->has_buffered_data(); }

void Router::set_pause_state(bool pause_state) {
  this->cached_pause_ = pause_state;
  this->get_active_output()->set_pause_state(pause_state);
}

void Router::set_volume(float volume) {
  this->volume_ = volume;
  this->get_active_output()->set_volume(volume);
}

void Router::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  this->get_active_output()->set_mute_state(mute_state);
}

bool Router::switch_to_output(speaker::Speaker *target) {
  if (target == nullptr) {
    return false;
  }

  int8_t new_idx = -1;
  for (size_t i = 0; i < this->outputs_.size(); i++) {
    if (this->outputs_[i] == target) {
      new_idx = static_cast<int8_t>(i);
      break;
    }
  }
  if (new_idx < 0) {
    ESP_LOGW(TAG, "Switch target is not a configured output");
    return false;
  }
  if (new_idx == this->active_output_idx_.load(std::memory_order_relaxed)) {
    return true;
  }

  // A switch is already in flight: pending_start_prev_idx_ is still releasing the
  // shared bus and the current active output's start() is still deferred (it never
  // started). Just redirect which output we start once the bus frees. Leave the bus
  // holder (pending_start_prev_idx_), the in-flight frame counter (loop() still owes one
  // synthetic credit for the bus holder's in-flight frames), and state_ alone, and
  // don't stop the current active output, which never started.
  if (this->pending_start_prev_idx_.load(std::memory_order_relaxed) >= 0) {
    this->active_output_idx_.store(new_idx, std::memory_order_relaxed);
    return true;
  }

  const bool was_active = (this->state_ == speaker::STATE_STARTING || this->state_ == speaker::STATE_RUNNING);
  const int8_t old_idx = this->active_output_idx_.load(std::memory_order_relaxed);

  if (was_active) {
    this->outputs_[old_idx]->stop();
  }

  this->active_output_idx_.store(new_idx, std::memory_order_relaxed);

  if (was_active) {
    // Defer start and the synthetic-credit callback until the old output's
    // task is fully stopped; loop() handles both. Firing the synthetic credit
    // here would race the old task's still-in-flight natural callbacks,
    // dispatching audio_output_callback_ concurrently from two threads, which
    // some consumers (e.g. sendspin's progress sync) aren't reentrant-safe for.
    // STATE_STOPPING keeps producers from observing a transient stopped state
    // and lets our play() short-circuit so the new output's play() doesn't
    // auto-start it while the shared bus is still being released.
    this->state_ = speaker::STATE_STOPPING;
    this->pending_start_prev_idx_.store(old_idx, std::memory_order_relaxed);
  } else {
    this->frames_in_pipeline_.store(0, std::memory_order_release);
  }
  return true;
}

void Router::apply_cached_state_to_active_() {
  speaker::Speaker *active = this->get_active_output();
  active->set_audio_stream_info(this->audio_stream_info_);
  active->set_volume(this->volume_);
  active->set_mute_state(this->mute_state_);
  active->set_pause_state(this->cached_pause_);
}

}  // namespace esphome::router

#endif  // USE_ESP32

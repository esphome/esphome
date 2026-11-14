#include "remote_receiver.h"

// The platform-independent part of timer sampling; remote_receiver_bk72xx.cpp reads the pin and the clock.
#ifdef USE_REMOTE_RECEIVER_TIMER_SAMPLING

namespace esphome::remote_receiver {

bool RemoteReceiverComponentStore::sample(bool level, uint32_t elapsed) {
  if (!this->capture_active) {
    if (level == this->last_level)
      return false;
    this->level_ticks = 0;
    this->last_level = level;
    // a frame starts with a mark
    if (level) {
      this->capture_active = true;
      this->run_active = false;
    }
    return false;
  }

  this->level_ticks += elapsed;
  // A space includes the runs the filter folded into it, so a glitch does not restart the idle count. It is
  // checked on an edge too, so a mark that arrives as the space reaches idle does not merge two frames.
  uint32_t idle_run = this->level_ticks;
  if (!this->last_level && this->run_active && !this->run_level)
    idle_run += this->run_ticks;
  if (idle_run >= this->idle_ticks)
    return this->end_frame(level);

  if (level == this->last_level)
    return false;
  this->finish_run(this->last_level, this->level_ticks);
  this->level_ticks = 0;
  this->last_level = level;
  return false;
}

bool RemoteReceiverComponentStore::end_frame(bool level) {
  // end the frame with one idle period, like the edge path, not the measured gap
  if (!this->last_level && this->run_active && !this->run_level) {
    this->run_ticks = this->idle_ticks;
  } else {
    this->finish_run(this->last_level, this->idle_ticks);
  }
  this->push_run();
  // a mark that ended the idle space starts the next capture
  this->capture_active = level && !this->last_level;
  this->run_active = false;
  this->level_ticks = 0;
  this->last_level = level;
  const uint16_t count = this->count;
  this->count = 0;
  // a capture that held only a glitch the filter folded away leaves nothing to dispatch
  if (count == 0)
    return false;
  if (this->frame_overflow) {
    this->frame_overflow = false;
    this->overflow = true;
    return true;
  }
  if (this->ready != nullptr) {
    this->busy = true;
    return true;
  }
  this->ready = this->capture;
  this->ready_count = count;
  this->capture = this->capture == this->buffer ? this->buffer + this->frame_entries : this->buffer;
  return true;
}

void RemoteReceiverComponentStore::finish_run(bool level, uint32_t ticks) {
  if (!this->run_active) {
    this->run_level = level;
    this->run_ticks = ticks;
    this->run_active = true;
    return;
  }
  // A run that returns to the pending level continues it, and a run shorter than the filter is folded into
  // it, so the emitted durations keep alternating sign as the decoders expect.
  if (level == this->run_level || ticks < this->filter_ticks) {
    this->run_ticks += ticks;
    return;
  }
  this->push_run();
  this->run_level = level;
  this->run_ticks = ticks;
}

void RemoteReceiverComponentStore::push_run() {
  // drop a leading space, for example one left after a first mark that the filter removed
  if (!this->run_active || (this->count == 0 && !this->run_level) || this->run_ticks < this->filter_ticks)
    return;
  const uint16_t count = this->count;
  if (count >= this->frame_entries) {
    // drop the rest of the frame; the frame end reports the overflow
    this->frame_overflow = true;
    return;
  }
  const auto run_us = static_cast<int32_t>(this->run_ticks / TIMER_TICKS_PER_US);
  this->capture[count] = this->run_level ? run_us : -run_us;
  this->count = count + 1;
}

}  // namespace esphome::remote_receiver

#endif  // USE_REMOTE_RECEIVER_TIMER_SAMPLING

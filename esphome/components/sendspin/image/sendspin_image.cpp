#include "sendspin_image.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_ARTWORK)

#include "esphome/core/log.h"

#include <cstring>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.image";

// How long a displayed frame may wait for sendspin.image.transition_finished before a warning
// names the missing ack. Generous next to a typical fade of a second or two.
static constexpr uint32_t TRANSITION_ACK_WARNING_MS = 10000;

// THREAD CONTEXT: Main loop. Children set up after the hub, so the artwork role already exists.
void SendspinImageSlot::setup() {
  const size_t frame_size = this->decode_sink_.get_buffer_size(this->width_, this->height_);
  if (frame_size == 0) {
    // The sink would refuse a buffer of these dimensions, so every decode would fall back to
    // allocating one of its own. Fail here instead, where the dimensions are already known.
    ESP_LOGE(TAG, "Cannot decode artwork at %dx%d", this->width_, this->height_);
    this->mark_failed();
    return;
  }

  RAMAllocator<uint8_t> allocator;
  for (uint8_t *&buffer : this->buffers_) {
    buffer = allocator.allocate(frame_size);
    if (buffer == nullptr) {
      ESP_LOGE(TAG, "Could not allocate %zu bytes for an artwork frame. Largest free block: %zu", frame_size,
               allocator.get_max_free_block_size());
      for (uint8_t *&allocated : this->buffers_) {
        allocator.deallocate(allocated, frame_size);
        allocated = nullptr;
      }
      this->mark_failed();
      return;
    }
    // Both buffers start black, so a transition has something to fade from before any artwork
    // has arrived.
    memset(buffer, 0, frame_size);
  }

  // Point both views at buffers_[current_index_] rather than the buffer the first decode writes
  // into, so they name a frame that stays black until artwork arrives.
  this->current_image_->set_frame(this->buffers_[this->current_index_], this->width_, this->height_);
  if (this->transition_image_ != nullptr) {
    this->transition_image_->set_frame(this->buffers_[this->current_index_], this->width_, this->height_);
  }

  this->parent_->add_image_decode_callback(
      [this](uint8_t slot, const uint8_t *data, size_t length, sendspin::SendspinImageFormat) {
        if (slot == this->slot_)
          this->on_decode_(data, length);
      });
  this->parent_->add_image_display_callback([this](uint8_t slot, uint32_t lateness_ms) {
    if (slot == this->slot_)
      this->on_display_(lateness_ms);
  });
  this->parent_->add_image_clear_callback([this](uint8_t slot) {
    if (slot == this->slot_)
      this->on_clear_();
  });
}

// THREAD CONTEXT: Dedicated artwork decode thread. The data pointer is valid only for this call.
void SendspinImageSlot::on_decode_(const uint8_t *data, size_t length) {
  uint8_t *target;
  {
    // The lock makes the main loop's last swap of current_index_ visible here. The frame_done gate
    // is what guarantees the buffer it picks out is not still needed by the main loop.
    LockGuard lock(this->pending_mutex_);
    target = this->buffers_[this->current_index_ ^ 1];
  }

  // The server letterboxes artwork onto a canvas of exactly the requested dimensions, so the sink
  // is pinned to them: a decode that asks for anything else is a malformed payload and drops the
  // frame.
  if (!this->decode_sink_.set_external_buffer(target, this->width_, this->height_)) {
    // setup() rules this out, but decoding without the handover would allocate a frame-sized
    // buffer on this thread, which is exactly what the permanent buffers exist to avoid.
    this->report_error_();
    return;
  }

  const bool decoded = this->decode_frame_(data, length, target);
  // Ends any half-finished decode session (the decoder object is kept for reuse). An external
  // buffer is let go of rather than freed, so this is safe on every path.
  this->decode_sink_.release();

  if (!decoded) {
    // The buffer keeps whatever the failed decode painted into it, but no view names it while a
    // decode can run, so nothing shows it.
    this->report_error_();
    return;
  }

  LockGuard lock(this->pending_mutex_);
  this->frame_pending_ = true;
}

// THREAD CONTEXT: Artwork decode thread, with target already handed to the sink.
bool SendspinImageSlot::decode_frame_(const uint8_t *data, size_t length, const uint8_t *target) {
  if (!this->decode_sink_.begin_decode(length)) {
    ESP_LOGE(TAG, "Could not start decode");
    return false;
  }

  size_t total_consumed = 0;
  while (total_consumed < length) {
    int consumed = this->decode_sink_.feed_data(const_cast<uint8_t *>(data) + total_consumed, length - total_consumed);
    if (consumed <= 0) {
      // <0 is a decode error; 0 means the decoder cannot make progress (truncated/corrupt data).
      ESP_LOGE(TAG, "Decode failed at offset %zu (result %d)", total_consumed, consumed);
      return false;
    }
    total_consumed += consumed;
  }

  if (!this->decode_sink_.end_decode()) {
    ESP_LOGE(TAG, "Could not finalize decode");
    return false;
  }

  // A decode that asked for other dimensions had the buffer taken away from it, so it painted
  // nothing (or stopped partway). JPEG and BMP report that as an error above; PNG carries on
  // regardless, so the frame is dropped here.
  return this->decode_sink_.decoded_into(target);
}

// THREAD CONTEXT: Main loop (fired once the slot's offset-shifted display deadline is reached).
void SendspinImageSlot::on_display_(uint32_t lateness_ms) {
  bool frame_ready;
  {
    LockGuard lock(this->pending_mutex_);
    frame_ready = this->frame_pending_;
    this->frame_pending_ = false;
    if (frame_ready) {
      // The decoded frame becomes the current one; the frame it replaces becomes the outgoing
      // frame, and the next decode target once the transition is acked.
      this->current_index_ ^= 1;
    }
  }
  if (!frame_ready) {
    // The decode for this display failed, so there is nothing new to show. The delivery still owes
    // its ack or the library would withhold every later frame for this slot.
    this->parent_->artwork_frame_done(this->slot_);
    return;
  }

  // The frame this display replaces is only real artwork if something was already on screen.
  const bool outgoing_is_artwork = this->showing_artwork_;
  this->showing_artwork_ = true;
  this->apply_frames_(outgoing_is_artwork);

  // Armed before the trigger fires so an automation that acks synchronously still counts, and armed
  // for the first frame too so the contract stays uniform: one transition_finished per display.
  this->transition_pending_ = this->transition_image_ != nullptr;
  if (this->transition_pending_) {
    // The library holds back further deliveries until the ack, with no timeout, so an automation
    // that never reaches the action stalls the slot with nothing in the log. Name the cause after
    // a generous wait. Arming again replaces the previous timeout, so it cannot fire for a frame
    // that was already acked and superseded.
    this->set_timeout("transition_ack", TRANSITION_ACK_WARNING_MS, [this]() {
      if (this->transition_pending_) {
        ESP_LOGW(TAG,
                 "Slot %u: displayed artwork was never acknowledged; no new artwork will arrive until "
                 "sendspin.image.transition_finished runs or the stream is cleared",
                 this->slot_);
      }
    });
  }
  this->image_display_callback_.call(lateness_ms);
  if (this->transition_image_ == nullptr) {
    this->finish_transition_();
  }
}

// THREAD CONTEXT: Main loop.
void SendspinImageSlot::finish_transition_() {
  this->transition_pending_ = false;
  if (this->transition_image_ != nullptr) {
    // Move it off the buffer the next decode writes into. What it shows does not change: the
    // buffer it moves to holds the artwork the transition just settled on.
    this->transition_image_->set_frame(this->buffers_[this->current_index_], this->width_, this->height_);
    this->transition_image_->set_showing_artwork(this->showing_artwork_);
  }
  // The ack wakes the decode thread, which may start writing buffers_[current_index_ ^ 1] straight
  // away, so nothing may still name that buffer by the time this runs.
  this->parent_->artwork_frame_done(this->slot_);
}

// THREAD CONTEXT: Main loop (invoked from the sendspin.image.transition_finished action).
void SendspinImageSlot::transition_finished() {
  if (!this->transition_pending_) {
    return;
  }
  this->finish_transition_();
}

// THREAD CONTEXT: Main loop (fired on stream end or clear for this slot).
void SendspinImageSlot::on_clear_() {
  {
    LockGuard lock(this->pending_mutex_);
    // Drop a frame that was decoded but never displayed; its buffer stays the decode target.
    this->frame_pending_ = false;
  }
  // No pixels are touched and the views keep naming the frames they had: a widget goes on drawing
  // the last artwork until the automation points it elsewhere or hides it. Only the display lambda
  // path stops drawing the artwork, falling back to the placeholder.
  this->current_image_->set_showing_artwork(false);
  if (this->transition_image_ != nullptr) {
    // Point it away from the decode target, as at setup, so it cannot show a frame being decoded.
    this->transition_image_->set_frame(this->buffers_[this->current_index_], this->width_, this->height_);
    this->transition_image_->set_showing_artwork(false);
  }
  this->showing_artwork_ = false;
  // Drops a running transition. Its automation cannot be cancelled here, so a late
  // transition_finished() can ack the next stream's first frame early, showing it without its
  // transition. The ack count stays right.
  this->transition_pending_ = false;
  this->image_clear_callback_.call();
  // A clear is itself a delivery owing exactly one ack, and it supersedes any un-acked frame --
  // including one whose transition never signalled transition_finished(), so a stalled slot
  // recovers here.
  this->parent_->artwork_frame_done(this->slot_);
}

// THREAD CONTEXT: Main loop.
void SendspinImageSlot::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Artwork slot %u:\n"
                "  Dimensions: %dx%d\n"
                "  Frame buffers: 2 x %zu bytes\n"
                "  Transition image: %s",
                this->slot_, this->width_, this->height_,
                this->decode_sink_.get_buffer_size(this->width_, this->height_),
                YESNO(this->transition_image_ != nullptr));
}

// THREAD CONTEXT: Main loop.
void SendspinImageSlot::apply_frames_(bool transition_is_artwork) {
  this->current_image_->set_frame(this->buffers_[this->current_index_], this->width_, this->height_);
  this->current_image_->set_showing_artwork(true);
  if (this->transition_image_ != nullptr) {
    this->transition_image_->set_frame(this->buffers_[this->current_index_ ^ 1], this->width_, this->height_);
    this->transition_image_->set_showing_artwork(transition_is_artwork);
  }
}

// THREAD CONTEXT: Artwork decode thread. Triggers must run on the main loop; defer() is thread-safe
// here because the hub enables wake_loop_threadsafe support.
void SendspinImageSlot::report_error_() {
  this->defer([this]() { this->image_error_callback_.call(); });
}

}  // namespace esphome::sendspin_

#endif

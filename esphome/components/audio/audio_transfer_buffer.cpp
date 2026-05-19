#include "audio_transfer_buffer.h"

#ifdef USE_ESP32

#include <cstring>

#include "esphome/core/helpers.h"

namespace esphome::audio {

AudioTransferBuffer::~AudioTransferBuffer() { this->deallocate_buffer_(); };

std::unique_ptr<AudioSinkTransferBuffer> AudioSinkTransferBuffer::create(size_t buffer_size) {
  std::unique_ptr<AudioSinkTransferBuffer> sink_buffer = make_unique<AudioSinkTransferBuffer>();

  if (!sink_buffer->allocate_buffer_(buffer_size)) {
    return nullptr;
  }

  return sink_buffer;
}

std::unique_ptr<AudioSourceTransferBuffer> AudioSourceTransferBuffer::create(size_t buffer_size) {
  std::unique_ptr<AudioSourceTransferBuffer> source_buffer = make_unique<AudioSourceTransferBuffer>();

  if (!source_buffer->allocate_buffer_(buffer_size)) {
    return nullptr;
  }

  return source_buffer;
}

size_t AudioTransferBuffer::free() const {
  if (this->buffer_size_ == 0) {
    return 0;
  }
  return this->buffer_size_ - (this->buffer_length_ + (this->data_start_ - this->buffer_));
}

void AudioTransferBuffer::decrease_buffer_length(size_t bytes) {
  this->buffer_length_ -= bytes;
  if (this->buffer_length_ > 0) {
    this->data_start_ += bytes;
  } else {
    // All the data in the buffer has been consumed, reset the start pointer
    this->data_start_ = this->buffer_;
  }
}

void AudioTransferBuffer::increase_buffer_length(size_t bytes) { this->buffer_length_ += bytes; }

void AudioTransferBuffer::clear_buffered_data() {
  this->buffer_length_ = 0;
  if (this->ring_buffer_.use_count() > 0) {
    this->ring_buffer_->reset();
  }
}

void AudioSinkTransferBuffer::clear_buffered_data() {
  this->buffer_length_ = 0;
  if (this->ring_buffer_.use_count() > 0) {
    this->ring_buffer_->reset();
  }
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    this->speaker_->stop();
  }
#endif
}

bool AudioTransferBuffer::has_buffered_data() const {
  if (this->ring_buffer_.use_count() > 0) {
    return ((this->ring_buffer_->available() > 0) || (this->available() > 0));
  }
  return (this->available() > 0);
}

bool AudioTransferBuffer::reallocate(size_t new_buffer_size) {
  if (this->buffer_ == nullptr) {
    return this->allocate_buffer_(new_buffer_size);
  }

  if (new_buffer_size < this->buffer_length_) {
    // New size is too small to hold existing data
    return false;
  }

  // Shift existing data to the start of the buffer so realloc preserves it
  if ((this->buffer_length_ > 0) && (this->data_start_ != this->buffer_)) {
    std::memmove(this->buffer_, this->data_start_, this->buffer_length_);
    this->data_start_ = this->buffer_;
  }

  RAMAllocator<uint8_t> allocator;
  uint8_t *new_buffer = allocator.reallocate(this->buffer_, new_buffer_size);
  if (new_buffer == nullptr) {
    // Reallocation failed, but the original buffer is still valid
    return false;
  }

  this->buffer_ = new_buffer;
  this->data_start_ = this->buffer_;
  this->buffer_size_ = new_buffer_size;
  return true;
}

bool AudioTransferBuffer::allocate_buffer_(size_t buffer_size) {
  this->buffer_size_ = buffer_size;

  RAMAllocator<uint8_t> allocator;

  this->buffer_ = allocator.allocate(this->buffer_size_);
  if (this->buffer_ == nullptr) {
    return false;
  }

  this->data_start_ = this->buffer_;
  this->buffer_length_ = 0;

  return true;
}

void AudioTransferBuffer::deallocate_buffer_() {
  if (this->buffer_ != nullptr) {
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->buffer_, this->buffer_size_);
    this->buffer_ = nullptr;
    this->data_start_ = nullptr;
  }

  this->buffer_size_ = 0;
  this->buffer_length_ = 0;
}

size_t AudioSourceTransferBuffer::transfer_data_from_source(TickType_t ticks_to_wait, bool pre_shift) {
  if (pre_shift) {
    // Shift data in buffer to start
    if (this->buffer_length_ > 0) {
      std::memmove(this->buffer_, this->data_start_, this->buffer_length_);
    }
    this->data_start_ = this->buffer_;
  }

  size_t bytes_to_read = AudioTransferBuffer::free();
  size_t bytes_read = 0;
  if (bytes_to_read > 0) {
    if (this->ring_buffer_.use_count() > 0) {
      bytes_read = this->ring_buffer_->read((void *) this->get_buffer_end(), bytes_to_read, ticks_to_wait);
    }

    this->increase_buffer_length(bytes_read);
  }
  return bytes_read;
}

size_t AudioSinkTransferBuffer::transfer_data_to_sink(TickType_t ticks_to_wait, bool post_shift) {
  size_t bytes_written = 0;
  if (this->available()) {
#ifdef USE_SPEAKER
    if (this->speaker_ != nullptr) {
      bytes_written = this->speaker_->play(this->data_start_, this->available(), ticks_to_wait);
    } else
#endif
        if (this->ring_buffer_.use_count() > 0) {
      bytes_written =
          this->ring_buffer_->write_without_replacement((void *) this->data_start_, this->available(), ticks_to_wait);
    } else if (this->sink_callback_ != nullptr) {
      bytes_written = this->sink_callback_->audio_sink_write(this->data_start_, this->available(), ticks_to_wait);
    }

    this->decrease_buffer_length(bytes_written);
  }

  if (post_shift) {
    // Shift unwritten data to the start of the buffer
    std::memmove(this->buffer_, this->data_start_, this->buffer_length_);
    this->data_start_ = this->buffer_;
  }

  return bytes_written;
}

bool AudioSinkTransferBuffer::has_buffered_data() const {
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    return (this->speaker_->has_buffered_data() || (this->available() > 0));
  }
#endif
  if (this->ring_buffer_.use_count() > 0) {
    return ((this->ring_buffer_->available() > 0) || (this->available() > 0));
  }
  return (this->available() > 0);
}

size_t AudioSourceTransferBuffer::free() const { return AudioTransferBuffer::free(); }

bool AudioSourceTransferBuffer::has_buffered_data() const { return AudioTransferBuffer::has_buffered_data(); }

void ConstAudioSourceBuffer::set_data(const uint8_t *data, size_t length) {
  this->data_start_ = data;
  this->length_ = length;
}

void ConstAudioSourceBuffer::consume(size_t bytes) {
  bytes = std::min(bytes, this->length_);
  this->length_ -= bytes;
  this->data_start_ += bytes;
}

std::unique_ptr<RingBufferAudioSource> RingBufferAudioSource::create(
    std::shared_ptr<ring_buffer::RingBuffer> ring_buffer, size_t max_fill_bytes, uint8_t alignment_bytes) {
  if (ring_buffer == nullptr || max_fill_bytes == 0 || alignment_bytes == 0 || alignment_bytes > MAX_ALIGNMENT_BYTES) {
    return nullptr;
  }
  return std::unique_ptr<RingBufferAudioSource>(
      new RingBufferAudioSource(std::move(ring_buffer), max_fill_bytes, alignment_bytes));
}

RingBufferAudioSource::~RingBufferAudioSource() {
  if (this->acquired_item_ != nullptr) {
    this->ring_buffer_->receive_release(this->acquired_item_);
    this->acquired_item_ = nullptr;
  }
}

void RingBufferAudioSource::release_item_() {
  if (this->acquired_item_ == nullptr) {
    return;
  }
  if (this->item_trailing_length_ > 0) {
    // Copy the trailing sub-frame bytes into the splice buffer before returning the item; the next
    // fill() will complete the frame from the head of the next chunk.
    std::memcpy(this->splice_buffer_, this->item_trailing_ptr_, this->item_trailing_length_);
    this->splice_length_ = this->item_trailing_length_;
    this->item_trailing_ptr_ = nullptr;
    this->item_trailing_length_ = 0;
  }
  this->ring_buffer_->receive_release(this->acquired_item_);
  this->acquired_item_ = nullptr;
}

void RingBufferAudioSource::consume(size_t bytes) {
  bytes = std::min(bytes, this->current_available_);
  this->current_data_ += bytes;
  this->current_available_ -= bytes;
  // Promotion of queued data is deferred to fill() so callers see new data as a fresh return value
  // rather than appearing silently after consume(). When the held item has nothing left depending
  // on it (no exposed bytes and no queued region), release it now so the ring buffer can be
  // reclaimed by writers even if fill() is never called again.
  if (this->current_available_ == 0 && this->queued_length_ == 0) {
    this->release_item_();
  }
}

bool RingBufferAudioSource::has_buffered_data() const {
  // splice_length_ is deliberately not considered here. It holds an incomplete frame whose completion
  // bytes must still arrive through the ring buffer, which ring_buffer_->available() already reports.
  // Counting it separately would strand a drain loop when a stream ends mid-frame and those completion
  // bytes never come.
  return (this->current_available_ > 0) || (this->queued_length_ > 0) || (this->ring_buffer_->available() > 0);
}

size_t RingBufferAudioSource::fill(TickType_t ticks_to_wait, bool /*pre_shift*/) {
  if (this->current_available_ > 0) {
    // Caller has not finished consuming the current exposure
    return 0;
  }

  // If a queued region (the aligned remainder of the new chunk after a splice frame) is waiting,
  // promote it to the exposed region and report its size as fresh data.
  if (this->queued_length_ > 0) {
    this->current_data_ = this->queued_data_;
    this->current_available_ = this->queued_length_;
    this->queued_data_ = nullptr;
    this->queued_length_ = 0;
    return this->current_available_;
  }

  // Nothing exposed and nothing queued: release the previously held item (saving any sub-frame tail
  // to splice_buffer_) and acquire a new chunk.
  this->release_item_();

  size_t chunk_length = 0;
  void *item = this->ring_buffer_->receive_acquire(chunk_length, this->max_fill_bytes_, ticks_to_wait);
  if (item == nullptr) {
    return 0;
  }

  uint8_t *chunk_data = static_cast<uint8_t *>(item);
  bool exposing_splice_frame = false;

  // Complete any pending splice frame from the head of the new chunk.
  if (this->splice_length_ > 0) {
    const size_t needed = static_cast<size_t>(this->alignment_bytes_) - this->splice_length_;
    if (chunk_length < needed) {
      // Not enough data to complete the spliced frame yet; absorb everything and wait for more.
      std::memcpy(this->splice_buffer_ + this->splice_length_, chunk_data, chunk_length);
      this->splice_length_ += chunk_length;
      this->ring_buffer_->receive_release(item);
      return 0;
    }
    std::memcpy(this->splice_buffer_ + this->splice_length_, chunk_data, needed);
    chunk_data += needed;
    chunk_length -= needed;
    this->splice_length_ = 0;
    exposing_splice_frame = true;
  }

  this->acquired_item_ = item;

  // Split the remaining chunk into its aligned region and a (possibly zero) sub-frame trailing tail.
  const size_t trailing = (this->alignment_bytes_ > 1) ? (chunk_length % this->alignment_bytes_) : 0;
  const size_t aligned_bytes = chunk_length - trailing;
  if (trailing > 0) {
    this->item_trailing_ptr_ = chunk_data + aligned_bytes;
    this->item_trailing_length_ = trailing;
  }

  if (exposing_splice_frame) {
    // Expose the spliced frame from splice_buffer_, queuing the chunk's aligned region for the next
    // fill() call.
    this->current_data_ = this->splice_buffer_;
    this->current_available_ = this->alignment_bytes_;
    this->queued_data_ = chunk_data;
    this->queued_length_ = aligned_bytes;
    return this->alignment_bytes_;
  }

  if (aligned_bytes == 0) {
    // The entire chunk is a sub-frame tail (only possible when alignment exceeds chunk size). Save it
    // to the splice buffer and release the item so the next fill() can complete the frame.
    this->release_item_();
    return 0;
  }

  this->current_data_ = chunk_data;
  this->current_available_ = aligned_bytes;
  return aligned_bytes;
}

}  // namespace esphome::audio

#endif

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

}  // namespace esphome::audio

#endif

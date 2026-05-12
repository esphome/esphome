#pragma once

#ifdef USE_ESP32
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/core/defines.h"

#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif

#include "esp_err.h"

#include <freertos/FreeRTOS.h>

namespace esphome::audio {

/// @brief Abstract interface for writing decoded audio data to a sink.
class AudioSinkCallback {
 public:
  virtual size_t audio_sink_write(uint8_t *data, size_t length, TickType_t ticks_to_wait) = 0;
};

class AudioTransferBuffer {
  /*
   * @brief Class that facilitates tranferring data between a buffer and an audio source or sink.
   * The transfer buffer is a typical C array that temporarily holds data for processing in other audio components.
   * Both sink and source transfer buffers can use a ring buffer as the sink/source.
   *   - The ring buffer is stored in a shared_ptr, so destroying the transfer buffer object will release ownership.
   */
 public:
  /// @brief Destructor that deallocates the transfer buffer
  ~AudioTransferBuffer();

  /// @brief Returns a pointer to the start of the transfer buffer where available() bytes of existing data can be read
  uint8_t *get_buffer_start() const { return this->data_start_; }

  /// @brief Returns a pointer to the end of the transfer buffer where free() bytes of new data can be written
  uint8_t *get_buffer_end() const { return this->data_start_ + this->buffer_length_; }

  /// @brief Updates the internal state of the transfer buffer. This should be called after reading data
  /// @param bytes The number of bytes consumed/read
  void decrease_buffer_length(size_t bytes);

  /// @brief Updates the internal state of the transfer buffer. This should be called after writing data
  /// @param bytes The number of bytes written
  void increase_buffer_length(size_t bytes);

  /// @brief Returns the transfer buffer's currently available bytes to read
  size_t available() const { return this->buffer_length_; }

  /// @brief Returns the transfer buffers allocated bytes
  size_t capacity() const { return this->buffer_size_; }

  /// @brief Returns the transfer buffer's currrently free bytes available to write
  size_t free() const;

  /// @brief Clears data in the transfer buffer and, if possible, the source/sink.
  virtual void clear_buffered_data();

  /// @brief Tests if there is any data in the tranfer buffer or the source/sink.
  /// @return True if there is data, false otherwise.
  virtual bool has_buffered_data() const;

  /// @brief Reallocates the transfer buffer, preserving any existing data.
  /// @param new_buffer_size The new size in bytes. Must be at least as large as available().
  /// @return True if successful, false otherwise. On failure, the original buffer remains valid.
  bool reallocate(size_t new_buffer_size);

 protected:
  /// @brief Allocates the transfer buffer in external memory, if available.
  /// @param buffer_size The number of bytes to allocate
  /// @return True is successful, false otherwise.
  bool allocate_buffer_(size_t buffer_size);

  /// @brief Deallocates the buffer and resets the class variables.
  void deallocate_buffer_();

  // A possible source or sink for the transfer buffer
  std::shared_ptr<ring_buffer::RingBuffer> ring_buffer_;

  uint8_t *buffer_{nullptr};
  uint8_t *data_start_{nullptr};

  size_t buffer_size_{0};
  size_t buffer_length_{0};
};

class AudioSinkTransferBuffer : public AudioTransferBuffer {
  /*
   * @brief A class that implements a transfer buffer for audio sinks.
   * Supports writing processed data in the transfer buffer to a ring buffer or a speaker component.
   */
 public:
  /// @brief Creates a new sink transfer buffer.
  /// @param buffer_size Size of the transfer buffer in bytes.
  /// @return unique_ptr if successfully allocated, nullptr otherwise
  static std::unique_ptr<AudioSinkTransferBuffer> create(size_t buffer_size);

  /// @brief Writes any available data in the transfer buffer to the sink.
  /// @param ticks_to_wait FreeRTOS ticks to block while waiting for the sink to have enough space
  /// @param post_shift If true, all remaining data is moved to the start of the buffer after transferring to the sink.
  ///                   Defaults to true.
  /// @return Number of bytes written
  size_t transfer_data_to_sink(TickType_t ticks_to_wait, bool post_shift = true);

  /// @brief Adds a ring buffer as the transfer buffer's sink.
  /// @param ring_buffer weak_ptr to the allocated ring buffer
  void set_sink(const std::weak_ptr<ring_buffer::RingBuffer> &ring_buffer) { this->ring_buffer_ = ring_buffer.lock(); }

#ifdef USE_SPEAKER
  /// @brief Adds a speaker as the transfer buffer's sink.
  /// @param speaker Pointer to the speaker component
  void set_sink(speaker::Speaker *speaker) { this->speaker_ = speaker; }
#endif

  /// @brief Adds a callback as the transfer buffer's sink.
  /// @param callback Pointer to the AudioSinkCallback implementation
  void set_sink(AudioSinkCallback *callback) { this->sink_callback_ = callback; }

  void clear_buffered_data() override;

  bool has_buffered_data() const override;

 protected:
#ifdef USE_SPEAKER
  speaker::Speaker *speaker_{nullptr};
#endif
  AudioSinkCallback *sink_callback_{nullptr};
};

/// @brief Abstract interface for reading audio data from a buffer.
/// Provides a common read interface for both mutable transfer buffers and read-only const buffers.
class AudioReadableBuffer {
 public:
  virtual ~AudioReadableBuffer() = default;

  /// @brief Returns a pointer to the start of readable data
  virtual const uint8_t *data() const = 0;

  /// @brief Returns the number of bytes available to read
  virtual size_t available() const = 0;

  /// @brief Returns the number of free bytes available to write. Defaults to 0 for read-only buffers.
  virtual size_t free() const { return 0; }

  /// @brief Advances past consumed data
  /// @param bytes Number of bytes consumed
  virtual void consume(size_t bytes) = 0;

  /// @brief Tests if there is any buffered data
  virtual bool has_buffered_data() const = 0;

  /// @brief Refills the buffer from its source. No-op by default for read-only buffers.
  /// @param ticks_to_wait FreeRTOS ticks to block while waiting for data
  /// @param pre_shift If true, shifts existing data to the start of the buffer before reading
  /// @return Number of bytes read
  virtual size_t fill(TickType_t ticks_to_wait, bool pre_shift) { return 0; }
  size_t fill(TickType_t ticks_to_wait) { return this->fill(ticks_to_wait, true); }
};

class AudioSourceTransferBuffer : public AudioTransferBuffer, public AudioReadableBuffer {
  /*
   * @brief A class that implements a transfer buffer for audio sources.
   * Supports reading audio data from a ring buffer into the transfer buffer for processing.
   * Implements AudioReadableBuffer for use by consumers that only need read access.
   */
 public:
  /// @brief Creates a new source transfer buffer.
  /// @param buffer_size Size of the transfer buffer in bytes.
  /// @return unique_ptr if successfully allocated, nullptr otherwise
  static std::unique_ptr<AudioSourceTransferBuffer> create(size_t buffer_size);

  /// @brief Reads any available data from the source into the transfer buffer.
  /// @param ticks_to_wait FreeRTOS ticks to block while waiting for the source to have enough data
  /// @param pre_shift If true, any unwritten data is moved to the start of the buffer before transferring from the
  ///                  source. Defaults to true.
  /// @return Number of bytes read
  size_t transfer_data_from_source(TickType_t ticks_to_wait, bool pre_shift = true);

  /// @brief Adds a ring buffer as the transfer buffer's source.
  /// @param ring_buffer weak_ptr to the allocated ring buffer
  void set_source(const std::weak_ptr<ring_buffer::RingBuffer> &ring_buffer) {
    this->ring_buffer_ = ring_buffer.lock();
  };

  // AudioReadableBuffer interface
  const uint8_t *data() const override { return this->data_start_; }
  size_t available() const override { return this->buffer_length_; }
  size_t free() const override;
  void consume(size_t bytes) override { this->decrease_buffer_length(bytes); }
  bool has_buffered_data() const override;
  size_t fill(TickType_t ticks_to_wait, bool pre_shift) override {
    return this->transfer_data_from_source(ticks_to_wait, pre_shift);
  }
};

/// @brief A lightweight read-only audio buffer for const data sources (e.g., flash memory).
/// Does not allocate memory or transfer data from external sources.
class ConstAudioSourceBuffer : public AudioReadableBuffer {
 public:
  /// @brief Sets the data pointer and length for the buffer
  /// @param data Pointer to the const audio data
  /// @param length Size of the data in bytes
  void set_data(const uint8_t *data, size_t length);

  // AudioReadableBuffer interface
  const uint8_t *data() const override { return this->data_start_; }
  size_t available() const override { return this->length_; }
  void consume(size_t bytes) override;
  bool has_buffered_data() const override { return this->length_ > 0; }

 protected:
  const uint8_t *data_start_{nullptr};
  size_t length_{0};
};

/// @brief Zero-copy audio source that reads directly from a ring buffer's internal storage.
///
/// Optionally enforces a minimum read alignment (e.g. one audio frame). When alignment_bytes > 1, the
/// source transparently stitches frames that straddle the ring buffer's wrap boundary by buffering the
/// trailing partial frame from one chunk and joining it with the head of the next chunk in a small
/// internal splice buffer, so callers always see frame-aligned data.
class RingBufferAudioSource : public AudioReadableBuffer {
 public:
  /// Maximum supported alignment. Sized to cover 32-bit samples across up to 2 channels (8 bytes).
  static constexpr size_t MAX_ALIGNMENT_BYTES = 8;

  /// @brief Creates a new ring-buffer-backed audio source after validating its parameters.
  /// @param ring_buffer The ring buffer to read from. Must be non-null.
  /// @param max_fill_bytes Soft cap on bytes acquired per fill() call. Must be > 0.
  /// @param alignment_bytes Minimum exposed-region alignment in bytes (defaults to 1, i.e. byte-aligned).
  ///        Pass bytes_per_frame to make every exposed region a whole number of frames. Must be in
  ///        [1, MAX_ALIGNMENT_BYTES].
  /// @return unique_ptr if parameters are valid, nullptr otherwise
  static std::unique_ptr<RingBufferAudioSource> create(std::shared_ptr<ring_buffer::RingBuffer> ring_buffer,
                                                       size_t max_fill_bytes, uint8_t alignment_bytes = 1);

  ~RingBufferAudioSource() override;

  // AudioReadableBuffer interface
  const uint8_t *data() const override { return this->current_data_; }
  size_t available() const override { return this->current_available_; }
  void consume(size_t bytes) override;
  bool has_buffered_data() const override;
  size_t fill(TickType_t ticks_to_wait, bool pre_shift) override;

  /// @brief Returns a mutable pointer to the currently exposed audio data.
  /// The pointer may reference the ring buffer's internal storage or, when exposing a stitched frame
  /// across a wrap boundary, an internal splice buffer. In either case mutations are safe but data
  /// should be discarded after use, since the underlying storage will be reused on the next fill().
  /// Use only when the caller is the sole consumer of this source.
  uint8_t *mutable_data() { return this->current_data_; }

 protected:
  /// @brief Constructs a new ring-buffer-backed audio source. Use create() instead, which validates
  /// arguments before construction.
  explicit RingBufferAudioSource(std::shared_ptr<ring_buffer::RingBuffer> ring_buffer, size_t max_fill_bytes,
                                 uint8_t alignment_bytes)
      : ring_buffer_(std::move(ring_buffer)), max_fill_bytes_(max_fill_bytes), alignment_bytes_(alignment_bytes) {}

  /// @brief Releases the currently held ring buffer item, first copying any trailing sub-frame bytes
  /// into the splice buffer so they can be stitched with the next chunk.
  void release_item_();

  std::shared_ptr<ring_buffer::RingBuffer> ring_buffer_;
  size_t max_fill_bytes_;

  void *acquired_item_{nullptr};
  uint8_t *current_data_{nullptr};

  // Sub-frame trailing bytes inside the held item that will be copied to splice_buffer_ on release.
  uint8_t *item_trailing_ptr_{nullptr};

  // After the currently-exposed splice frame is consumed, fill() will promote this region (the aligned
  // remainder of the new chunk) to the exposed region. queued_length_ == 0 when nothing is queued.
  uint8_t *queued_data_{nullptr};

  // Splice buffer holds the start of a partial frame whose remainder lives at the head of the next
  // chunk. While splice_length_ > 0, the buffer is incomplete and waiting for completion bytes.
  uint8_t splice_buffer_[MAX_ALIGNMENT_BYTES];

  size_t current_available_{0};
  size_t queued_length_{0};

  // item_trailing_length_ and splice_length_ are bounded by MAX_ALIGNMENT_BYTES.
  uint8_t alignment_bytes_;
  uint8_t item_trailing_length_{0};
  uint8_t splice_length_{0};
};

}  // namespace esphome::audio

#endif

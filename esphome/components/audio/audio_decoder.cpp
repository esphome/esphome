#include "audio_decoder.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"

namespace esphome {
namespace audio {

static const uint32_t DECODING_TIMEOUT_MS = 50;    // The decode function will yield after this duration
static const uint32_t READ_WRITE_TIMEOUT_MS = 20;  // Timeout for transferring audio data

static const uint32_t MAX_POTENTIALLY_FAILED_COUNT = 10;

AudioDecoder::AudioDecoder(size_t input_buffer_size, size_t output_buffer_size) {
  this->input_transfer_buffer_ = AudioSourceTransferBuffer::create(input_buffer_size);
  this->output_transfer_buffer_ = AudioSinkTransferBuffer::create(output_buffer_size);
}

AudioDecoder::~AudioDecoder() {
#ifdef USE_AUDIO_MP3_SUPPORT
  if (this->audio_file_type_ == AudioFileType::MP3) {
    esp_audio_libs::helix_decoder::MP3FreeDecoder(this->mp3_decoder_);
  }
#endif
}

esp_err_t AudioDecoder::add_source(std::weak_ptr<RingBuffer> &input_ring_buffer) {
  if (this->input_transfer_buffer_ != nullptr) {
    this->input_transfer_buffer_->set_source(input_ring_buffer);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

esp_err_t AudioDecoder::add_sink(std::weak_ptr<RingBuffer> &output_ring_buffer) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(output_ring_buffer);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

#ifdef USE_SPEAKER
esp_err_t AudioDecoder::add_sink(speaker::Speaker *speaker) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(speaker);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}
#endif

esp_err_t AudioDecoder::start(AudioFileType audio_file_type) {
  if ((this->input_transfer_buffer_ == nullptr) || (this->output_transfer_buffer_ == nullptr)) {
    return ESP_ERR_NO_MEM;
  }

  this->audio_file_type_ = audio_file_type;

  this->potentially_failed_count_ = 0;
  this->end_of_file_ = false;

  switch (this->audio_file_type_) {
#ifdef USE_AUDIO_FLAC_SUPPORT
    case AudioFileType::FLAC:
      this->flac_decoder_ = make_unique<esp_audio_libs::flac::FLACDecoder>();
      this->free_buffer_required_ =
          this->output_transfer_buffer_->capacity();  // Adjusted and reallocated after reading the header
      break;
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
    case AudioFileType::MP3:
      this->mp3_decoder_ = esp_audio_libs::helix_decoder::MP3InitDecoder();

      // MP3 always has 1152 samples per chunk
      this->free_buffer_required_ = 1152 * sizeof(int16_t) * 2;  // samples * size per sample * channels

      // Always reallocate the output transfer buffer to the smallest necessary size
      this->output_transfer_buffer_->reallocate(this->free_buffer_required_);
      break;
#endif
    case AudioFileType::WAV:
      this->wav_decoder_ = make_unique<esp_audio_libs::wav_decoder::WAVDecoder>();
      this->wav_decoder_->reset();

      // Processing WAVs doesn't actually require a specific amount of buffer size, as it is already in PCM format.
      // Thus, we don't reallocate to a minimum size.
      this->free_buffer_required_ = 1024;
      if (this->output_transfer_buffer_->capacity() < this->free_buffer_required_) {
        this->output_transfer_buffer_->reallocate(this->free_buffer_required_);
      }
      break;
    case AudioFileType::NONE:
    default:
      return ESP_ERR_NOT_SUPPORTED;
      break;
  }

  return ESP_OK;
}

AudioDecoderState AudioDecoder::decode(bool stop_gracefully) {
  if (stop_gracefully) {
    if (this->output_transfer_buffer_->available() == 0) {
      if (this->end_of_file_) {
        // The file decoder indicates it reached the end of file
        return AudioDecoderState::FINISHED;
      }

      if (!this->input_transfer_buffer_->has_buffered_data()) {
        // If all the internal buffers are empty, the decoding is done
        return AudioDecoderState::FINISHED;
      }
    }
  }

  if (this->potentially_failed_count_ > MAX_POTENTIALLY_FAILED_COUNT) {
    if (stop_gracefully) {
      // No more new data is going to come in, so decoding is done
      return AudioDecoderState::FINISHED;
    }
    return AudioDecoderState::FAILED;
  }

  FileDecoderState state = FileDecoderState::MORE_TO_PROCESS;

  uint32_t decoding_start = millis();

  bool first_loop_iteration = true;

  size_t bytes_processed = 0;
  size_t bytes_available_before_processing = 0;

  while (state == FileDecoderState::MORE_TO_PROCESS) {
    // Transfer decoded out
    if (!this->pause_output_) {
      // Never shift the data in the output transfer buffer to avoid unnecessary, slow data moves
      size_t bytes_written =
          this->output_transfer_buffer_->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);

      if (this->audio_stream_info_.has_value()) {
        this->accumulated_frames_written_ += this->audio_stream_info_.value().bytes_to_frames(bytes_written);
        this->playback_ms_ +=
            this->audio_stream_info_.value().frames_to_milliseconds_with_remainder(&this->accumulated_frames_written_);
      }
    } else {
      // If paused, block to avoid wasting CPU resources
      delay(READ_WRITE_TIMEOUT_MS);
    }

    // Verify there is enough space to store more decoded audio and that the function hasn't been running too long
    if ((this->output_transfer_buffer_->free() < this->free_buffer_required_) ||
        (millis() - decoding_start > DECODING_TIMEOUT_MS)) {
      return AudioDecoderState::DECODING;
    }

    // Decode more audio

    // Only shift data on the first loop iteration to avoid unnecessary, slow moves
    size_t bytes_read = this->input_transfer_buffer_->transfer_data_from_source(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS),
                                                                                first_loop_iteration);

    if (!first_loop_iteration && (this->input_transfer_buffer_->available() < bytes_processed)) {
      // Less data is available than what was processed in last iteration, so don't attempt to decode.
      // This attempts to avoid the decoder from consistently trying to decode an incomplete frame. The transfer buffer
      // will shift the remaining data to the start and copy more from the source the next time the decode function is
      // called
      break;
    }

    bytes_available_before_processing = this->input_transfer_buffer_->available();

    if ((this->potentially_failed_count_ > 0) && (bytes_read == 0)) {
      // Failed to decode in last attempt and there is no new data

      if ((this->input_transfer_buffer_->free() == 0) && first_loop_iteration) {
        // The input buffer is full. Since it previously failed on the exact same data, we can never recover
        state = FileDecoderState::FAILED;
      } else {
        // Attempt to get more data next time
        state = FileDecoderState::IDLE;
      }
    } else if (this->input_transfer_buffer_->available() == 0) {
      // No data to decode, attempt to get more data next time
      state = FileDecoderState::IDLE;
    } else {
      switch (this->audio_file_type_) {
#ifdef USE_AUDIO_FLAC_SUPPORT
        case AudioFileType::FLAC:
          state = this->decode_flac_();
          break;
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
        case AudioFileType::MP3:
          state = this->decode_mp3_();
          break;
#endif
        case AudioFileType::WAV:
          state = this->decode_wav_();
          break;
        case AudioFileType::NONE:
        default:
          state = FileDecoderState::IDLE;
          break;
      }
    }

    first_loop_iteration = false;
    bytes_processed = bytes_available_before_processing - this->input_transfer_buffer_->available();

    if (state == FileDecoderState::POTENTIALLY_FAILED) {
      ++this->potentially_failed_count_;
    } else if (state == FileDecoderState::END_OF_FILE) {
      this->end_of_file_ = true;
    } else if (state == FileDecoderState::FAILED) {
      return AudioDecoderState::FAILED;
    } else if (state == FileDecoderState::MORE_TO_PROCESS) {
      this->potentially_failed_count_ = 0;
    }
  }
  return AudioDecoderState::DECODING;
}

#ifdef USE_AUDIO_FLAC_SUPPORT
FileDecoderState AudioDecoder::decode_flac_() {
  if (!this->audio_stream_info_.has_value()) {
    // Header hasn't been read
    auto result = this->flac_decoder_->read_header(this->input_transfer_buffer_->get_buffer_start(),
                                                   this->input_transfer_buffer_->available());

    if (result == esp_audio_libs::flac::FLAC_DECODER_HEADER_OUT_OF_DATA) {
      return FileDecoderState::POTENTIALLY_FAILED;
    }

    if (result != esp_audio_libs::flac::FLAC_DECODER_SUCCESS) {
      // Couldn't read FLAC header
      return FileDecoderState::FAILED;
    }

    size_t bytes_consumed = this->flac_decoder_->get_bytes_index();
    this->input_transfer_buffer_->decrease_buffer_length(bytes_consumed);

    // Reallocate the output transfer buffer to the smallest necessary size
    this->free_buffer_required_ = flac_decoder_->get_output_buffer_size_bytes();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      // Couldn't reallocate output buffer
      return FileDecoderState::FAILED;
    }

    this->audio_stream_info_ =
        audio::AudioStreamInfo(this->flac_decoder_->get_sample_depth(), this->flac_decoder_->get_num_channels(),
                               this->flac_decoder_->get_sample_rate());

    return FileDecoderState::MORE_TO_PROCESS;
  }

  uint32_t output_samples = 0;
  auto result = this->flac_decoder_->decode_frame(
      this->input_transfer_buffer_->get_buffer_start(), this->input_transfer_buffer_->available(),
      reinterpret_cast<int16_t *>(this->output_transfer_buffer_->get_buffer_end()), &output_samples);

  if (result == esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
    // Not an issue, just needs more data that we'll get next time.
    return FileDecoderState::POTENTIALLY_FAILED;
  }

  size_t bytes_consumed = this->flac_decoder_->get_bytes_index();
  this->input_transfer_buffer_->decrease_buffer_length(bytes_consumed);

  if (result > esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
    // Corrupted frame, don't retry with current buffer content, wait for new sync
    return FileDecoderState::POTENTIALLY_FAILED;
  }

  // We have successfully decoded some input data and have new output data
  this->output_transfer_buffer_->increase_buffer_length(
      this->audio_stream_info_.value().samples_to_bytes(output_samples));

  if (result == esp_audio_libs::flac::FLAC_DECODER_NO_MORE_FRAMES) {
    return FileDecoderState::END_OF_FILE;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_MP3_SUPPORT
FileDecoderState AudioDecoder::decode_mp3_() {
  // Look for the next sync word
  int buffer_length = (int) this->input_transfer_buffer_->available();
  int32_t offset =
      esp_audio_libs::helix_decoder::MP3FindSyncWord(this->input_transfer_buffer_->get_buffer_start(), buffer_length);

  if (offset < 0) {
    // New data may have the sync word
    this->input_transfer_buffer_->decrease_buffer_length(buffer_length);
    return FileDecoderState::POTENTIALLY_FAILED;
  }

  // Advance read pointer to match the offset for the syncword
  this->input_transfer_buffer_->decrease_buffer_length(offset);
  uint8_t *buffer_start = this->input_transfer_buffer_->get_buffer_start();

  buffer_length = (int) this->input_transfer_buffer_->available();
  int err = esp_audio_libs::helix_decoder::MP3Decode(this->mp3_decoder_, &buffer_start, &buffer_length,
                                                     (int16_t *) this->output_transfer_buffer_->get_buffer_end(), 0);

  size_t consumed = this->input_transfer_buffer_->available() - buffer_length;
  this->input_transfer_buffer_->decrease_buffer_length(consumed);

  if (err) {
    switch (err) {
      case esp_audio_libs::helix_decoder::ERR_MP3_OUT_OF_MEMORY:
        // Intentional fallthrough
      case esp_audio_libs::helix_decoder::ERR_MP3_NULL_POINTER:
        return FileDecoderState::FAILED;
        break;
      default:
        // Most errors are recoverable by moving on to the next frame, so mark as potentailly failed
        return FileDecoderState::POTENTIALLY_FAILED;
        break;
    }
  } else {
    esp_audio_libs::helix_decoder::MP3FrameInfo mp3_frame_info;
    esp_audio_libs::helix_decoder::MP3GetLastFrameInfo(this->mp3_decoder_, &mp3_frame_info);
    if (mp3_frame_info.outputSamps > 0) {
      int bytes_per_sample = (mp3_frame_info.bitsPerSample / 8);
      this->output_transfer_buffer_->increase_buffer_length(mp3_frame_info.outputSamps * bytes_per_sample);

      if (!this->audio_stream_info_.has_value()) {
        this->audio_stream_info_ =
            audio::AudioStreamInfo(mp3_frame_info.bitsPerSample, mp3_frame_info.nChans, mp3_frame_info.samprate);
      }
    }
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

FileDecoderState AudioDecoder::decode_wav_() {
  if (!this->audio_stream_info_.has_value()) {
    // Header hasn't been processed

    esp_audio_libs::wav_decoder::WAVDecoderResult result = this->wav_decoder_->decode_header(
        this->input_transfer_buffer_->get_buffer_start(), this->input_transfer_buffer_->available());

    if (result == esp_audio_libs::wav_decoder::WAV_DECODER_SUCCESS_IN_DATA) {
      this->input_transfer_buffer_->decrease_buffer_length(this->wav_decoder_->bytes_processed());

      this->audio_stream_info_ = audio::AudioStreamInfo(
          this->wav_decoder_->bits_per_sample(), this->wav_decoder_->num_channels(), this->wav_decoder_->sample_rate());

      this->wav_bytes_left_ = this->wav_decoder_->chunk_bytes_left();
      this->wav_has_known_end_ = (this->wav_bytes_left_ > 0);
      return FileDecoderState::MORE_TO_PROCESS;
    } else if (result == esp_audio_libs::wav_decoder::WAV_DECODER_WARNING_INCOMPLETE_DATA) {
      // Available data didn't have the full header
      return FileDecoderState::POTENTIALLY_FAILED;
    } else {
      return FileDecoderState::FAILED;
    }
  } else {
    if (!this->wav_has_known_end_ || (this->wav_bytes_left_ > 0)) {
      size_t bytes_to_copy = this->input_transfer_buffer_->available();

      if (this->wav_has_known_end_) {
        bytes_to_copy = std::min(bytes_to_copy, this->wav_bytes_left_);
      }

      bytes_to_copy = std::min(bytes_to_copy, this->output_transfer_buffer_->free());

      if (bytes_to_copy > 0) {
        std::memcpy(this->output_transfer_buffer_->get_buffer_end(), this->input_transfer_buffer_->get_buffer_start(),
                    bytes_to_copy);
        this->input_transfer_buffer_->decrease_buffer_length(bytes_to_copy);
        this->output_transfer_buffer_->increase_buffer_length(bytes_to_copy);
        if (this->wav_has_known_end_) {
          this->wav_bytes_left_ -= bytes_to_copy;
        }
      }
      return FileDecoderState::IDLE;
    }
  }

  return FileDecoderState::END_OF_FILE;
}

}  // namespace audio
}  // namespace esphome

#endif

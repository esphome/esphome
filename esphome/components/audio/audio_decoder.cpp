#include "audio_decoder.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::audio {

static const char *const TAG = "audio.decoder";

static const uint32_t DECODING_TIMEOUT_MS = 50;    // The decode function will yield after this duration
static const uint32_t READ_WRITE_TIMEOUT_MS = 20;  // Timeout for transferring audio data

static const uint32_t MAX_POTENTIALLY_FAILED_COUNT = 10;

AudioDecoder::AudioDecoder(size_t input_buffer_size, size_t output_buffer_size)
    : input_buffer_size_(input_buffer_size) {
  this->output_transfer_buffer_ = AudioSinkTransferBuffer::create(output_buffer_size);
}

esp_err_t AudioDecoder::add_source(std::weak_ptr<ring_buffer::RingBuffer> &input_ring_buffer) {
  auto source = AudioSourceTransferBuffer::create(this->input_buffer_size_);
  if (source == nullptr) {
    return ESP_ERR_NO_MEM;
  }
  source->set_source(input_ring_buffer);
  this->input_buffer_ = std::move(source);
  return ESP_OK;
}

esp_err_t AudioDecoder::add_source(const uint8_t *data_pointer, size_t length) {
  auto source = make_unique<ConstAudioSourceBuffer>();
  source->set_data(data_pointer, length);
  this->input_buffer_ = std::move(source);
  return ESP_OK;
}

esp_err_t AudioDecoder::add_sink(std::weak_ptr<ring_buffer::RingBuffer> &output_ring_buffer) {
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

esp_err_t AudioDecoder::add_sink(AudioSinkCallback *callback) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(callback);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

esp_err_t AudioDecoder::start(AudioFileType audio_file_type) {
  if (this->output_transfer_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  this->audio_file_type_ = audio_file_type;

  this->potentially_failed_count_ = 0;
  this->end_of_file_ = false;

  switch (this->audio_file_type_) {
#ifdef USE_AUDIO_FLAC_SUPPORT
    case AudioFileType::FLAC:
      this->flac_decoder_ = make_unique<micro_flac::FLACDecoder>();
      this->free_buffer_required_ =
          this->output_transfer_buffer_->capacity();  // Adjusted and reallocated after reading the header
      break;
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
    case AudioFileType::MP3:
      this->mp3_decoder_ = make_unique<micro_mp3::Mp3Decoder>();
      this->free_buffer_required_ =
          this->output_transfer_buffer_->capacity();  // Adjusted and reallocated after reading the header
      break;
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
    case AudioFileType::OPUS:
      this->opus_decoder_ = make_unique<micro_opus::OggOpusDecoder>();
      this->free_buffer_required_ =
          this->output_transfer_buffer_->capacity();  // Adjusted and reallocated after reading the header
      break;
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
    case AudioFileType::WAV:
      this->wav_decoder_ = make_unique<micro_wav::WAVDecoder>();
      // 1 KiB suffices to always make progress while avoiding excessive CPU spinning for decoding
      this->free_buffer_required_ = 1024;
      if (this->output_transfer_buffer_->capacity() < this->free_buffer_required_) {
        this->output_transfer_buffer_->reallocate(this->free_buffer_required_);
      }
      break;
#endif
    case AudioFileType::NONE:
    default:
      return ESP_ERR_NOT_SUPPORTED;
      break;
  }

  return ESP_OK;
}

AudioDecoderState AudioDecoder::decode(bool stop_gracefully) {
  if (this->input_buffer_ == nullptr) {
    return AudioDecoderState::FAILED;
  }

  if (stop_gracefully) {
    if (this->output_transfer_buffer_->available() == 0) {
      if (this->end_of_file_) {
        // The file decoder indicates it reached the end of file
        return AudioDecoderState::FINISHED;
      }

      if (!this->input_buffer_->has_buffered_data()) {
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

    // Never shift the input buffer; every decoder buffers internally and consumes only what it processed.
    size_t bytes_read = this->input_buffer_->fill(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);

    if (!first_loop_iteration && (this->input_buffer_->available() < bytes_processed)) {
      // Less data is available than what was processed in last iteration, so don't attempt to decode.
      // This attempts to avoid the decoder from consistently trying to decode an incomplete frame. The transfer buffer
      // will shift the remaining data to the start and copy more from the source the next time the decode function is
      // called
      break;
    }

    bytes_available_before_processing = this->input_buffer_->available();

    if ((this->potentially_failed_count_ > 0) && (bytes_read == 0)) {
      // Failed to decode in last attempt and there is no new data

      if ((this->input_buffer_->free() == 0) && first_loop_iteration) {
        // The input buffer is full (or read-only, e.g. const flash source). Since it previously failed on the exact
        // same data, we can never recover. For const sources this is correct: the entire file is already available, so
        // a decode failure is genuine, not a transient out-of-data condition.
        state = FileDecoderState::FAILED;
      } else {
        // Attempt to get more data next time
        state = FileDecoderState::IDLE;
      }
    } else if (this->input_buffer_->available() == 0) {
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
#ifdef USE_AUDIO_OPUS_SUPPORT
        case AudioFileType::OPUS:
          state = this->decode_opus_();
          break;
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
        case AudioFileType::WAV:
          state = this->decode_wav_();
          break;
#endif
        case AudioFileType::NONE:
        default:
          state = FileDecoderState::IDLE;
          break;
      }
    }

    first_loop_iteration = false;
    bytes_processed = bytes_available_before_processing - this->input_buffer_->available();

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
  size_t bytes_consumed, samples_decoded;

  micro_flac::FLACDecoderResult result = this->flac_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  if (result == micro_flac::FLAC_DECODER_SUCCESS) {
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().samples_to_bytes(samples_decoded));
    }
    this->input_buffer_->consume(bytes_consumed);
  } else if (result == micro_flac::FLAC_DECODER_HEADER_READY) {
    // Header just parsed, stream info now available
    const auto &info = this->flac_decoder_->get_stream_info();
    this->audio_stream_info_ = audio::AudioStreamInfo(info.bits_per_sample(), info.num_channels(), info.sample_rate());

    // Reallocate the output transfer buffer to the required size
    this->free_buffer_required_ = this->flac_decoder_->get_output_buffer_size_samples() * info.bytes_per_sample();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
    this->input_buffer_->consume(bytes_consumed);
  } else if (result == micro_flac::FLAC_DECODER_END_OF_STREAM) {
    this->input_buffer_->consume(bytes_consumed);
    return FileDecoderState::END_OF_FILE;
  } else if (result == micro_flac::FLAC_DECODER_NEED_MORE_DATA) {
    this->input_buffer_->consume(bytes_consumed);
    return FileDecoderState::MORE_TO_PROCESS;
  } else if (result == micro_flac::FLAC_DECODER_ERROR_OUTPUT_TOO_SMALL) {
    // Reallocate to decode the frame on the next call
    const auto &info = this->flac_decoder_->get_stream_info();
    this->free_buffer_required_ = this->flac_decoder_->get_output_buffer_size_samples() * info.bytes_per_sample();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
  } else {
    ESP_LOGE(TAG, "FLAC decoder failed: %d", static_cast<int>(result));
    return FileDecoderState::POTENTIALLY_FAILED;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_MP3_SUPPORT
FileDecoderState AudioDecoder::decode_mp3_() {
  // microMP3's samples_decoded value is samples per channel; e.g., what ESPHome typically calls an audio frame.
  // microMP3 uses the term frame to refer to an MP3 frame: an encoded packet that contains multiple audio frames.
  size_t bytes_consumed = 0;
  size_t samples_decoded = 0;

  // microMP3 buffers internally: it consumes from our input buffer at its own pace, emits MP3_STREAM_INFO_READY once
  // the first frame header is parsed, and only then produces PCM. It handles sync-word search and ID3v2 tag skipping.
  micro_mp3::Mp3Result result = this->mp3_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  this->input_buffer_->consume(bytes_consumed);

  if (result == micro_mp3::MP3_OK) {
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().frames_to_bytes(samples_decoded));
    }
  } else if (result == micro_mp3::MP3_STREAM_INFO_READY) {
    // First successful header parse: capture stream info and resize the output buffer to fit one full frame.
    // microMP3 always outputs 16-bit PCM.
    this->audio_stream_info_ =
        audio::AudioStreamInfo(16, this->mp3_decoder_->get_channels(), this->mp3_decoder_->get_sample_rate());
    this->free_buffer_required_ =
        this->mp3_decoder_->get_samples_per_frame() * this->mp3_decoder_->get_channels() * sizeof(int16_t);
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
  } else if (result == micro_mp3::MP3_NEED_MORE_DATA) {
    return FileDecoderState::MORE_TO_PROCESS;
  } else if (result == micro_mp3::MP3_OUTPUT_BUFFER_TOO_SMALL) {
    // Reallocate to decode the frame on the next call
    if (this->mp3_decoder_->get_channels() > 0) {
      this->free_buffer_required_ =
          this->mp3_decoder_->get_samples_per_frame() * this->mp3_decoder_->get_channels() * sizeof(int16_t);
    } else {
      // Fallback to worst-case size if channel info isn't available
      this->free_buffer_required_ = this->mp3_decoder_->get_min_output_buffer_bytes();
    }
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      return FileDecoderState::FAILED;
    }
  } else if (result == micro_mp3::MP3_DECODE_ERROR) {
    // Corrupt frame skipped; recoverable, retry on next call
    ESP_LOGW(TAG, "MP3 decoder skipped a corrupt frame");
    return FileDecoderState::POTENTIALLY_FAILED;
  } else {
    // MP3_ALLOCATION_FAILED, MP3_INPUT_INVALID, or any future error -- not recoverable
    ESP_LOGE(TAG, "MP3 decoder failed: %d", static_cast<int>(result));
    return FileDecoderState::FAILED;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_OPUS_SUPPORT
FileDecoderState AudioDecoder::decode_opus_() {
  bool processed_header = this->opus_decoder_->is_initialized();

  size_t bytes_consumed, samples_decoded;

  micro_opus::OggOpusResult result = this->opus_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  if (result == micro_opus::OGG_OPUS_OK) {
    if (!processed_header && this->opus_decoder_->is_initialized()) {
      // Header processed and stream info is available
      this->audio_stream_info_ =
          audio::AudioStreamInfo(this->opus_decoder_->get_bit_depth(), this->opus_decoder_->get_channels(),
                                 this->opus_decoder_->get_sample_rate());
    }
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      // Some audio was processed
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().frames_to_bytes(samples_decoded));
    }
    this->input_buffer_->consume(bytes_consumed);
  } else if (result == micro_opus::OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL) {
    // Reallocate to decode the packet on the next call
    this->free_buffer_required_ = this->opus_decoder_->get_required_output_buffer_size();
    if (!this->output_transfer_buffer_->reallocate(this->free_buffer_required_)) {
      // Couldn't reallocate output buffer
      return FileDecoderState::FAILED;
    }
  } else {
    ESP_LOGE(TAG, "Opus decoder failed: %" PRId8, result);
    return FileDecoderState::POTENTIALLY_FAILED;
  }
  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

#ifdef USE_AUDIO_WAV_SUPPORT
FileDecoderState AudioDecoder::decode_wav_() {
  // microWAV's samples_decoded counts individual channel samples; e.g., for
  // 16-bit stereo, 4 input bytes results in 2 samples_decoded.
  size_t bytes_consumed = 0;
  size_t samples_decoded = 0;

  micro_wav::WAVDecoderResult result = this->wav_decoder_->decode(
      this->input_buffer_->data(), this->input_buffer_->available(), this->output_transfer_buffer_->get_buffer_end(),
      this->output_transfer_buffer_->free(), bytes_consumed, samples_decoded);

  this->input_buffer_->consume(bytes_consumed);

  if (result == micro_wav::WAV_DECODER_SUCCESS) {
    if (samples_decoded > 0 && this->audio_stream_info_.has_value()) {
      this->output_transfer_buffer_->increase_buffer_length(
          this->audio_stream_info_.value().samples_to_bytes(samples_decoded));
    }
  } else if (result == micro_wav::WAV_DECODER_HEADER_READY) {
    // After HEADER_READY, get_bits_per_sample() returns the output bit depth
    // (16 for A-law/mu-law, 32 for IEEE float, original value for PCM).
    this->audio_stream_info_ =
        audio::AudioStreamInfo(this->wav_decoder_->get_bits_per_sample(), this->wav_decoder_->get_channels(),
                               this->wav_decoder_->get_sample_rate());
  } else if (result == micro_wav::WAV_DECODER_NEED_MORE_DATA) {
    return FileDecoderState::MORE_TO_PROCESS;
  } else if (result == micro_wav::WAV_DECODER_END_OF_STREAM) {
    return FileDecoderState::END_OF_FILE;
  } else {
    ESP_LOGE(TAG, "WAV decoder failed: %d", static_cast<int>(result));
    return FileDecoderState::FAILED;
  }

  return FileDecoderState::MORE_TO_PROCESS;
}
#endif

}  // namespace esphome::audio

#endif

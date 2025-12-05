#include "sendspin_decoder.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/core/log.h"

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.decoder";

void SendspinDecoder::reset_decoders() {
  this->flac_decoder_.reset();

  if (this->opus_decoder_ != nullptr) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
    allocator.deallocate((uint8_t *) this->opus_decoder_, this->opus_decoder_size_);
    this->opus_decoder_ = nullptr;
    this->opus_decoder_size_ = 0;
  }

  this->current_codec_ = SendspinCodecFormat::UNSUPPORTED;
}

bool SendspinDecoder::process_header(std::shared_ptr<SendspinAudioChunk> header_chunk,
                                     audio::AudioStreamInfo *stream_info) {
  if (header_chunk == nullptr || stream_info == nullptr) {
    ESP_LOGE(TAG, "Null pointer passed to process_header");
    return false;
  }

  switch (header_chunk->chunk_type) {
    case CHUNK_TYPE_FLAC_HEADER: {
      this->flac_decoder_ = make_unique<esp_audio_libs::flac::FLACDecoder>();

      auto result = this->flac_decoder_->read_header(header_chunk->get_data(),
                                                     header_chunk->size);  // get_data() already applies offset

      if (result == esp_audio_libs::flac::FLAC_DECODER_HEADER_OUT_OF_DATA) {
        ESP_LOGW(TAG, "Need more data to decode FLAC header");
        return false;
      }

      if (result != esp_audio_libs::flac::FLAC_DECODER_SUCCESS) {
        ESP_LOGE(TAG, "Serious error decoding FLAC header");
        return false;
      }
      this->current_codec_ = SendspinCodecFormat::FLAC;
      this->current_stream_info_ =
          audio::AudioStreamInfo(this->flac_decoder_->get_sample_depth(), this->flac_decoder_->get_num_channels(),
                                 this->flac_decoder_->get_sample_rate());
      *stream_info = this->current_stream_info_;
      this->maximum_decoded_size_ = this->flac_decoder_->get_output_buffer_size_bytes();
      break;
      // return true;
    }
    case CHUNK_TYPE_OPUS_DUMMY_HEADER: {
      if (!this->decode_dummy_header_(header_chunk, stream_info)) {
        return false;
      }

      auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);

      this->opus_decoder_size_ = opus_decoder_get_size(stream_info->get_channels());
      this->opus_decoder_ = (OpusDecoder *) allocator.allocate(this->opus_decoder_size_);

      auto decoder_error =
          opus_decoder_init(this->opus_decoder_, stream_info->get_sample_rate(), stream_info->get_channels());

      if (decoder_error != OPUS_OK) {
        ESP_LOGE(TAG, "Failed to create OPUS decoder, error %d", decoder_error);

        if (this->opus_decoder_ != nullptr) {
          allocator.deallocate((uint8_t *) this->opus_decoder_, this->opus_decoder_size_);
          this->opus_decoder_ = nullptr;
        }
        return false;
      }

      this->maximum_decoded_size_ = stream_info->ms_to_bytes(120);  // Opus max frame size is 120ms
      this->current_stream_info_ = *stream_info;
      this->current_codec_ = SendspinCodecFormat::OPUS;
      break;
    }
    case CHUNK_TYPE_PCM_DUMMY_HEADER: {
      if (!this->decode_dummy_header_(header_chunk, stream_info)) {
        return false;
      }
      this->current_stream_info_ = *stream_info;
      this->current_codec_ = SendspinCodecFormat::PCM;
      break;
    }
    default: {
      ESP_LOGE(TAG, "Audio chunk isn't a codec header");
      return false;
    }
  }

  // Caller retains ownership of header_chunk
  return true;
}

bool SendspinDecoder::decode_audio_chunk(std::shared_ptr<SendspinAudioChunk> encoded_chunk,
                                         std::shared_ptr<SendspinAudioChunk> &decoded_chunk) {
  if (encoded_chunk == nullptr) {
    ESP_LOGE(TAG, "Null pointer passed to decode_audio_chunk");
    return false;
  }

  if (this->current_codec_ == SendspinCodecFormat::PCM) {
    // For PCM, no decoding needed - share the same chunk
    decoded_chunk = encoded_chunk;
    // shared_ptr automatically handles reference counting
  } else {
    // For other codecs, allocate new chunk and decode
    decoded_chunk = create_sendspin_chunk(this->maximum_decoded_size_);
    if (decoded_chunk == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate space for decoded audio");
      return false;
    }

    if ((this->flac_decoder_ != nullptr) && (this->current_codec_ == SendspinCodecFormat::FLAC)) {
      uint32_t output_samples = 0;
      auto result = this->flac_decoder_->decode_frame(encoded_chunk->get_data(), encoded_chunk->get_usable_size(),
                                                      decoded_chunk->get_data(), &output_samples);

      if (result == esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
        ESP_LOGE(TAG, "FLAC decoder ran out of data");
        decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
        return false;
      }

      if (result > esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
        ESP_LOGE(TAG, "Serious error decoding FLAC file");
        decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
        return false;
      }

      decoded_chunk->offset = 0;
      decoded_chunk->size = this->current_stream_info_.samples_to_bytes(output_samples);
      // Try to shrink buffer to save memory (only works if we're the sole owner)
      audio::shrink_audio_chunk_buffer(decoded_chunk);
    } else if ((this->opus_decoder_ != nullptr) && (this->current_codec_ == SendspinCodecFormat::OPUS)) {
      int output_frames = opus_decode(this->opus_decoder_, encoded_chunk->get_data(), encoded_chunk->get_usable_size(),
                                      (int16_t *) decoded_chunk->get_data(),
                                      this->current_stream_info_.bytes_to_frames(this->maximum_decoded_size_), 0);
      if (output_frames < 0) {
        ESP_LOGE(TAG, "Error decoding opus chunk: %d", output_frames);
        decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
        return false;
      }

      decoded_chunk->offset = 0;
      decoded_chunk->size = this->current_stream_info_.frames_to_bytes(output_frames);
      // Try to shrink buffer to save memory (only works if we're the sole owner)
      audio::shrink_audio_chunk_buffer(decoded_chunk);
    } else {
      decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
      return false;
    }
  }

  decoded_chunk->chunk_type = CHUNK_TYPE_DECODED_AUDIO;

  return true;
}

bool SendspinDecoder::decode_dummy_header_(std::shared_ptr<SendspinAudioChunk> header_chunk,
                                           audio::AudioStreamInfo *stream_info) {
  // TODO: why doesn't this work... may have been fixed since last tested
  //   if (header_chunk->size != sizeof(DummyHeader)) {
  //     ESP_LOGE(TAG, "Invalid dummy codec header");
  //     return false;
  //   }

  DummyHeader *header = reinterpret_cast<DummyHeader *>(header_chunk->get_data());
  this->current_stream_info_ = audio::AudioStreamInfo(header->bits_per_sample, header->channels, header->sample_rate);
  *stream_info = this->current_stream_info_;
  return true;
}

}  // namespace sendspin
}  // namespace esphome

#endif

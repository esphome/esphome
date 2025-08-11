#include "resonate_decoder.h"

#if defined(USE_ESP_IDF) && defined(USE_RESONATE_AUDIO)

#include "esphome/core/log.h"

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.decoder";

void ResonateDecoder::reset_decoders() {
  this->flac_decoder_.reset();

  if (this->opus_decoder_ != nullptr) {
    auto allocator = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE);
    allocator.deallocate((uint8_t *) this->opus_decoder_, this->opus_decoder_size_);
    this->opus_decoder_ = nullptr;
    this->opus_decoder_size_ = 0;
  }

  this->current_codec_ = ResonateCodecFormat::RESONATE_CODEC_UNSUPPORTED;
}

bool ResonateDecoder::process_header(AudioChunk *header_chunk, audio::AudioStreamInfo *stream_info) {
  switch (header_chunk->chunk_type) {
    case CHUNK_TYPE_FLAC_HEADER: {
      this->flac_decoder_ = make_unique<esp_audio_libs::flac::FLACDecoder>();

      auto result = this->flac_decoder_->read_header(
          header_chunk->data + header_chunk->offset,
          header_chunk->size);  // TODO: is .size correct or do I need to account for the offset?

      if (result == esp_audio_libs::flac::FLAC_DECODER_HEADER_OUT_OF_DATA) {
        ESP_LOGW(TAG, "Need more data to decode FLAC header");
        return false;
      }

      if (result != esp_audio_libs::flac::FLAC_DECODER_SUCCESS) {
        ESP_LOGE(TAG, "Serious error decoding FLAC header");
        return false;
      }
      this->current_codec_ = ResonateCodecFormat::RESONATE_CODEC_FLAC;
      this->current_stream_info_ =
          audio::AudioStreamInfo(this->flac_decoder_->get_sample_depth(), this->flac_decoder_->get_num_channels(),
                                 this->flac_decoder_->get_sample_rate());
      *stream_info = this->current_stream_info_;
      break;
      // return true;
    }
    case CHUNK_TYPE_OPUS_DUMMY_HEADER: {
      if (!this->decode_dummy_header_(*header_chunk, stream_info)) {
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

      this->current_codec_ = ResonateCodecFormat::RESONATE_CODEC_OPUS;
      break;
    }
    case CHUNK_TYPE_PCM_DUMMY_HEADER: {
      this->current_codec_ = ResonateCodecFormat::RESONATE_CODEC_PCM;
      if (!this->decode_dummy_header_(*header_chunk, stream_info)) {
        return false;
      }
      break;
    }
    default: {
      ESP_LOGE(TAG, "Audio chunk isn't a codec header");
      return false;
    }
  }

  deallocate_audio_chunk(header_chunk);
  return true;
}

bool ResonateDecoder::decode_audio_chunk(AudioChunk *encoded_chunk, AudioChunk *decoded_chunk) {
  if (this->current_codec_ != ResonateCodecFormat::RESONATE_CODEC_PCM) {
    // Allocate memory to store decoded output. PCM audio is already decoded, so we instead copy the pointer
    if (!allocate_audio_chunk(this->current_stream_info_.frames_to_bytes(encoded_chunk->frame_count), decoded_chunk)) {
      ESP_LOGE(TAG, "Failed to allocate space for decoded audio");
      return false;
    }
  }

  if ((this->flac_decoder_ != nullptr) && (this->current_codec_ == ResonateCodecFormat::RESONATE_CODEC_FLAC)) {
    uint32_t output_samples = 0;
    auto result = this->flac_decoder_->decode_frame(encoded_chunk->data + encoded_chunk->offset, encoded_chunk->size,
                                                    reinterpret_cast<int16_t *>(decoded_chunk->data), &output_samples);

    if (result == esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
      ESP_LOGE(TAG, "FLAC decoder ran out of data");
      return false;
    }

    if (result > esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
      ESP_LOGE(TAG, "Serious error decoding FLAC file");
      return false;
    }

    decoded_chunk->offset = 0;
    decoded_chunk->size = this->current_stream_info_.samples_to_bytes(output_samples);
    deallocate_audio_chunk(encoded_chunk);
  } else if ((this->opus_decoder_ != nullptr) && (this->current_codec_ == ResonateCodecFormat::RESONATE_CODEC_OPUS)) {
    int output_frames =
        opus_decode(this->opus_decoder_, (encoded_chunk->data + encoded_chunk->offset), encoded_chunk->size,
                    (int16_t *) decoded_chunk->data, encoded_chunk->frame_count, 0);
    if (output_frames < 0) {
      ESP_LOGE(TAG, "Error decoding opus chunk: %d", output_frames);
      return false;
    }

    decoded_chunk->offset = 0;
    decoded_chunk->size = this->current_stream_info_.frames_to_bytes(output_frames);
    deallocate_audio_chunk(encoded_chunk);
  } else if (this->current_codec_ == ResonateCodecFormat::RESONATE_CODEC_PCM) {
    decoded_chunk->data = encoded_chunk->data;
    decoded_chunk->offset = encoded_chunk->offset;
    decoded_chunk->size = encoded_chunk->size;
  } else {
    return false;
  }

  decoded_chunk->frame_count = encoded_chunk->frame_count;
  decoded_chunk->chunk_type = CHUNK_TYPE_DECODED_AUDIO;

  return true;
}

bool ResonateDecoder::decode_dummy_header_(const AudioChunk &header_chunk, audio::AudioStreamInfo *stream_info) {
  // TODO: why doesn't this work... may have been fixed since last tested
  //   if (header_chunk.size != sizeof(DummyHeader)) {
  //     ESP_LOGE(TAG, "Invalid dummy codec header");
  //     return false;
  //   }

  DummyHeader *header = reinterpret_cast<DummyHeader *>(header_chunk.data + header_chunk.offset);
  this->current_stream_info_ = audio::AudioStreamInfo(header->bits_per_sample, header->channels, header->sample_rate);
  *stream_info = this->current_stream_info_;
  return true;
}

}  // namespace resonate
}  // namespace esphome

#endif

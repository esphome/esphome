#include "sendspin_decoder.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/core/log.h"

#include <cstring>

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

bool SendspinDecoder::process_header(const uint8_t *data, size_t data_size, ChunkType chunk_type,
                                     audio::AudioStreamInfo *stream_info) {
  if (data == nullptr || stream_info == nullptr) {
    ESP_LOGE(TAG, "Null pointer passed to process_header");
    return false;
  }

  switch (chunk_type) {
    case CHUNK_TYPE_FLAC_HEADER: {
      this->flac_decoder_ = make_unique<esp_audio_libs::flac::FLACDecoder>();
      this->flac_decoder_->set_crc_check_enabled(false);  // Disable CRC check for small speed up

      auto result = this->flac_decoder_->read_header(data, data_size);

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
    }
    case CHUNK_TYPE_OPUS_DUMMY_HEADER: {
      if (!this->decode_dummy_header_(data, data_size, stream_info)) {
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
      if (!this->decode_dummy_header_(data, data_size, stream_info)) {
        return false;
      }
      this->current_stream_info_ = *stream_info;
      this->current_codec_ = SendspinCodecFormat::PCM;
      this->maximum_decoded_size_ = stream_info->ms_to_bytes(120);  // PCM max chunk size
      break;
    }
    default: {
      ESP_LOGE(TAG, "Audio chunk isn't a codec header");
      return false;
    }
  }

  return true;
}

bool SendspinDecoder::decode_audio_chunk(const uint8_t *data, size_t data_size, uint8_t *output_buffer,
                                         size_t output_buffer_size, size_t *decoded_size) {
  if (data == nullptr || data_size == 0 || output_buffer == nullptr || decoded_size == nullptr) {
    ESP_LOGE(TAG, "Invalid data passed to decode_audio_chunk");
    return false;
  }

  if (this->current_codec_ == SendspinCodecFormat::PCM) {
    if (data_size > output_buffer_size) {
      ESP_LOGE(TAG, "PCM data size %zu exceeds output buffer size %zu", data_size, output_buffer_size);
      return false;
    }
    std::memcpy(output_buffer, data, data_size);
    *decoded_size = data_size;
  } else if ((this->flac_decoder_ != nullptr) && (this->current_codec_ == SendspinCodecFormat::FLAC)) {
    uint32_t output_samples = 0;
    auto result = this->flac_decoder_->decode_frame(data, data_size, output_buffer, &output_samples);

    if (result == esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
      ESP_LOGE(TAG, "FLAC decoder ran out of data");
      return false;
    }

    if (result > esp_audio_libs::flac::FLAC_DECODER_ERROR_OUT_OF_DATA) {
      ESP_LOGE(TAG, "Serious error decoding FLAC file");
      return false;
    }

    *decoded_size = this->current_stream_info_.samples_to_bytes(output_samples);
  } else if ((this->opus_decoder_ != nullptr) && (this->current_codec_ == SendspinCodecFormat::OPUS)) {
    int output_frames = opus_decode(this->opus_decoder_, data, data_size, (int16_t *) output_buffer,
                                    this->current_stream_info_.bytes_to_frames(output_buffer_size), 0);
    if (output_frames < 0) {
      ESP_LOGE(TAG, "Error decoding opus chunk: %d", output_frames);
      return false;
    }

    *decoded_size = this->current_stream_info_.frames_to_bytes(output_frames);
  } else {
    return false;
  }

  return true;
}

bool SendspinDecoder::decode_dummy_header_(const uint8_t *data, size_t data_size, audio::AudioStreamInfo *stream_info) {
  if (data_size < sizeof(DummyHeader)) {
    ESP_LOGE(TAG, "Invalid dummy codec header: size %zu < %zu", data_size, sizeof(DummyHeader));
    return false;
  }

  // Copy into local struct to avoid alignment issues
  DummyHeader header;
  std::memcpy(&header, data, sizeof(DummyHeader));
  this->current_stream_info_ = audio::AudioStreamInfo(header.bits_per_sample, header.channels, header.sample_rate);
  *stream_info = this->current_stream_info_;
  return true;
}

}  // namespace sendspin
}  // namespace esphome

#endif

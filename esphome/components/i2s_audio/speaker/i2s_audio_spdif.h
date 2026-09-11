#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_I2S_AUDIO_SPDIF_MODE)

#include "i2s_audio_speaker.h"
#include "spdif_encoder.h"

namespace esphome::i2s_audio {

/// @brief SPDIF speaker implementation.
/// Encodes PCM audio into IEC 60958-3 S/PDIF bitstream using BMC encoding,
/// outputting through a single I2S data pin. Maintains continuous output
/// (silence when no audio) to keep SPDIF receivers synchronized.
class I2SAudioSpeakerSPDIF : public I2SAudioSpeakerBase {
 public:
  void setup() override;
  void dump_config() override;

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;

 protected:
  void run_speaker_task() override;
  esp_err_t start_i2s_driver(audio::AudioStreamInfo &audio_stream_info) override;
  void on_task_stopped() override;

  SPDIFEncoder *spdif_encoder_{nullptr};
  uint32_t spdif_silence_start_{0};  // Timestamp when silence mode started (0 = not in silence)
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32 && USE_I2S_AUDIO_SPDIF_MODE

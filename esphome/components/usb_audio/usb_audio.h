#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

extern "C" {
#include "usb_stream.h"
}

namespace esphome {
namespace usb_audio {

static constexpr uint32_t USB_AUDIO_DEFAULT_BUFFER_SIZE = 6400;

struct AudioEndpointConfig {
  uint8_t channels{UAC_CH_ANY};
  uint16_t bits_per_sample{UAC_BITS_ANY};
  uint32_t sample_rate{UAC_FREQUENCY_ANY};
  uint32_t buffer_size{0};
  bool configured{false};
};

class USBAudioMicrophone;
class USBAudioSpeaker;

class USBAudioComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override {}
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_connect_timeout(uint32_t timeout_ms) { this->connect_timeout_ms_ = timeout_ms; }
  void set_microphone_buffer_size(uint32_t size) { this->mic_config_.buffer_size = size; }
  void set_speaker_buffer_size(uint32_t size) { this->speaker_config_.buffer_size = size; }

  void set_microphone_params(uint8_t channels, uint16_t bits, uint32_t sample_rate);
  void set_speaker_params(uint8_t channels, uint16_t bits, uint32_t sample_rate);

  void set_microphone(USBAudioMicrophone *microphone) { this->microphone_ = microphone; }
  void set_speaker(USBAudioSpeaker *speaker) { this->speaker_ = speaker; }

  bool ensure_started();
  void resume_microphone();
  void suspend_microphone();
  void resume_speaker();
  void suspend_speaker();

  esp_err_t read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms);
  esp_err_t write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms);

  uint32_t get_microphone_buffer_size() const { return this->mic_config_.buffer_size; }
  uint32_t get_speaker_buffer_size() const { return this->speaker_config_.buffer_size; }

  bool device_connected() const { return this->device_connected_; }

 protected:
  void configure_streams_();
  void handle_state_change_(usb_stream_state_t state);

  static void state_callback_(usb_stream_state_t state, void *user_data);

  AudioEndpointConfig mic_config_{};
  AudioEndpointConfig speaker_config_{};

  USBAudioMicrophone *microphone_{nullptr};
  USBAudioSpeaker *speaker_{nullptr};

  bool stream_started_{false};
  bool device_connected_{false};
  uint32_t connect_timeout_ms_{5000};
};

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)

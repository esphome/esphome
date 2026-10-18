#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO) && \
    (defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3))

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"

#include <atomic>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::usb_audio {

class USBAudioMicrophone : public microphone::Microphone, public Component, public Parented<USBAudioComponent> {
 public:
  void setup() override;
  void dump_config() override;

  void start() override;
  void stop() override;

  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_bits_per_sample(uint16_t bits) { this->bits_per_sample_ = bits; }
  void set_channels(uint8_t channels) { this->channels_ = channels; }
  void add_enabled_channel(uint8_t channel) { this->enabled_channel_mask_ |= 1U << channel; }

 protected:
  static void mic_task(void *param);
  void mic_task_loop_();
  void apply_channel_selection_(uint8_t *data, size_t length);

  static constexpr uint32_t READ_TIMEOUT_MS = 20;
  static constexpr uint32_t MIC_TASK_STACK_SIZE = 4096;
  static constexpr UBaseType_t MIC_TASK_PRIORITY = 18;

  // The data callback takes a std::vector, so this is reserved once in setup() and only ever resized within its
  // capacity by the mic task.
  std::vector<uint8_t> frame_;
  // Created once in setup() and parked between streams.
  StaticTask mic_task_;
  uint32_t sample_rate_{16000};
  uint16_t bits_per_sample_{16};
  uint8_t channels_{1};
  uint8_t enabled_channel_mask_{0};
  std::atomic<bool> running_{false};
  std::atomic<bool> task_idle_{true};
  bool logged_invalid_frame_{false};
};

}  // namespace esphome::usb_audio

#endif

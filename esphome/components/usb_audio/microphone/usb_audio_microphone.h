#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <deque>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace esphome {
namespace usb_audio {

class USBAudioMicrophone : public microphone::Microphone, public Component, public Parented<USBAudioComponent> {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void start() override;
  void stop() override;

  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_bits_per_sample(uint16_t bits) { this->bits_per_sample_ = bits; }
  void set_channels(uint8_t channels) { this->channels_ = channels; }
  void add_enabled_channel(uint8_t channel) { this->enabled_channels_.push_back(channel); }

 protected:
  static void mic_task_(void *param);
  void mic_task_loop_();
  void enqueue_frame_(const uint8_t *data, size_t length);
  uint8_t get_effective_channel_count_() const;

  bool running_{false};
  TaskHandle_t task_handle_{nullptr};

  Mutex queue_mutex_;
  std::deque<std::vector<uint8_t>> pending_frames_;

  std::vector<uint8_t> read_buffer_;
  std::vector<uint8_t> enabled_channels_;

  uint32_t sample_rate_{16000};
  uint16_t bits_per_sample_{16};
  uint8_t channels_{1};

  static constexpr size_t READ_TIMEOUT_MS = 20;
  static constexpr size_t MIC_TASK_STACK_SIZE = 4096;
  static constexpr UBaseType_t MIC_TASK_PRIORITY = 18;
  static constexpr size_t MAX_QUEUED_FRAMES = 8;

 private:
  bool logged_invalid_frame_ = false;
  bool logged_invalid_channel_ = false;
};

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)

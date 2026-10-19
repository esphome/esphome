#pragma once

#include "esphome/core/defines.h"

#ifdef USE_USB_AUDIO

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"

#include <atomic>
#include <vector>

namespace esphome::usb_audio {

class USBAudioMicrophone : public microphone::Microphone,
                           public Component,
                           public Parented<USBAudioComponent>,
                           public USBAudioStream {
 public:
  USBAudioMicrophone(const audio::AudioStreamInfo &stream_info, uint32_t buffer_size) : USBAudioStream(buffer_size) {
    this->audio_stream_info_ = stream_info;
  }

  void setup() override;
  void dump_config() override;
  void loop() override;

  // Safe from any task; loop() acts on the request.
  void start() override;
  void stop() override;

  audio::AudioStreamInfo get_usb_stream_info() override { return this->audio_stream_info_; }
  void on_stream_state(StreamEvent event) override;
  bool stream_idle() const override { return this->task_idle_; }

 protected:
  static constexpr uint8_t COMMAND_START = 1 << 0;
  static constexpr uint8_t COMMAND_STOP = 1 << 1;
  // Same limit as i2s_audio's microphone.
  static constexpr uint8_t MAX_LISTENERS = 16;
  static constexpr uint32_t READ_TIMEOUT_MS = 20;
  // A failed start is retried while listeners are held, backing off so a device that keeps failing does not flood the
  // log.
  static constexpr uint32_t RETRY_MIN_MS = 1000;
  static constexpr uint32_t RETRY_MAX_MS = 30000;
  // Scheduler ids.
  static constexpr uint32_t RETRY_START_ID = 0;
  static constexpr uint32_t MIC_TASK_STACK_SIZE = 4096;
  static constexpr UBaseType_t MIC_TASK_PRIORITY = 18;

  static void mic_task(void *param);
  void mic_task_loop_();
  void request_(uint8_t set, uint8_t clear);

  // The data callback takes a std::vector, so this is reserved once in setup() and only ever resized within its
  // capacity by the mic task.
  std::vector<uint8_t> frame_;
  // Created once in setup() and parked between streams.
  StaticTask mic_task_;
  std::atomic<uint8_t> commands_{0};
  // start() calls not yet matched by a stop(); capture stops when the last listener stops.
  std::atomic<uint8_t> listeners_{0};
  std::atomic<bool> running_{false};
  std::atomic<bool> task_idle_{true};
  // Main loop only.
  uint32_t retry_delay_ms_{RETRY_MIN_MS};
};

}  // namespace esphome::usb_audio

#endif

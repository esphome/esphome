#pragma once
#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::fakemic {

// Publishes 32 ms chunks of a uint16 counter (one count per sample) from its own task while started.
class FakeMicrophone : public microphone::Microphone, public Component {
 public:
  explicit FakeMicrophone(uint32_t rate) : rate_(rate) {}
  void setup() override {
    this->audio_stream_info_ = audio::AudioStreamInfo(16, 1, this->rate_);
    if (xTaskCreate(task_, "fakemic", 4096, this, 18, nullptr) != pdPASS)
      this->mark_failed(LOG_STR("fakemic task not created"));
  }
  void start() override {
    this->starts_++;
    this->state_ = microphone::STATE_RUNNING;
  }
  void stop() override {
    this->stops_++;
    this->state_ = microphone::STATE_STOPPED;
  }
  void stall(uint32_t ms) { this->stall_until_ = millis() + ms; }
  uint32_t starts_{0}, stops_{0}, chunks_{0};

 protected:
  static void task_(void *arg) {
    auto *self = static_cast<FakeMicrophone *>(arg);
    const uint32_t n = self->rate_ * 32 / 1000;
    auto *buf = new int16_t[n];
    TickType_t last = xTaskGetTickCount();
    uint16_t counter = 0;
    while (true) {
      vTaskDelayUntil(&last, pdMS_TO_TICKS(32));
      if (self->state_ != microphone::STATE_RUNNING || (int32_t) (millis() - self->stall_until_) < 0)
        continue;
      for (uint32_t i = 0; i < n; i++)
        buf[i] = (int16_t) counter++;
      self->publish_audio_(std::span<uint8_t>(reinterpret_cast<uint8_t *>(buf), n * 2), self->audio_stream_info_);
      self->chunks_++;
    }
  }
  uint32_t rate_;
  volatile uint32_t stall_until_{0};
};

}  // namespace esphome::fakemic

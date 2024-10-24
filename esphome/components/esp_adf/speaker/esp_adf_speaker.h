#pragma once

#ifdef USE_ESP_IDF

#include "../esp_adf.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <audio_element.h>
#include <audio_pipeline.h>

namespace esphome {
namespace esp_adf {

class ESPADFSpeaker : public ESPADFPipeline, public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void setup() override;
  void loop() override;

  void start() override;
  void stop() override;

  size_t play(const uint8_t *data, size_t length) override;

  bool has_buffered_data() const override;

 protected:
  void start_();
  void watch_();

  static void player_task(void *params);

  TaskHandle_t player_task_handle_{nullptr};
  struct {
    QueueHandle_t handle;
    uint8_t *storage;
  } buffer_queue_;
  QueueHandle_t event_queue_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

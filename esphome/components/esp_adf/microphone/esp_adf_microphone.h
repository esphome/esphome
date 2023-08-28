#pragma once

#ifdef USE_ESP_IDF

#include "../esp_adf.h"

#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

#include <audio_element.h>
#include <audio_pipeline.h>
#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <ringbuf.h>

namespace esphome {
namespace esp_adf {

class ESPADFMicrophone : public ESPADFPipeline, public microphone::Microphone, public Component {
 public:
  void setup() override;
  void start() override;
  void stop() override;

  void loop() override;

  size_t read(int16_t *buf, size_t len) override;

 protected:
  void start_();
  void stop_();
  void read_();
  void watch_();

  static void feed_task(void *params);
  static void fetch_task(void *params);

  const esp_afe_sr_iface_t *afe_handle_{&ESP_AFE_SR_HANDLE};
  esp_afe_sr_data_t *afe_data_{nullptr};
  size_t afe_chunk_size_{0};

  ringbuf_handle_t ring_buffer_;

  TaskHandle_t feed_task_handle_{nullptr};
  QueueHandle_t feed_event_queue_;
  QueueHandle_t feed_command_queue_;

  TaskHandle_t fetch_task_handle_{nullptr};
  QueueHandle_t fetch_command_queue_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

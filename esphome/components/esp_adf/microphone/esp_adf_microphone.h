#pragma once

#ifdef USE_ESP_IDF

#include "../esp_adf.h"

#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

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
  void read_();
  void watch_();

  static void read_task(void *params);

  ringbuf_handle_t ring_buffer_;

  TaskHandle_t read_task_handle_{nullptr};
  QueueHandle_t read_event_queue_;
  QueueHandle_t read_command_queue_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

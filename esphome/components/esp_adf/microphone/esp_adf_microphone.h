#pragma once

#ifdef USE_ESP32

#include "../esp_adf.h"

#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

#include <audio_element.h>
#include <audio_pipeline.h>

namespace esphome {
namespace esp_adf {

class ESPADFMicrophone : public ESPADFPipeline, public microphone::Microphone, public Component {
 public:
  void start() override;
  void stop() override;

  void loop() override;

  size_t read(int16_t *buf, size_t len) override;

 protected:
  void start_();
  void stop_();
  void read_();

  audio_pipeline_handle_t pipeline_;
  audio_element_handle_t i2s_stream_reader_, filter_, raw_read_;
};

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP32

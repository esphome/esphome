#include "esp_adf_speaker.h"

#ifdef USE_ESP_IDF

#include <driver/i2s.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <audio_hal.h>
#include <filter_resample.h>
#include <i2s_stream.h>
#include <raw_stream.h>

namespace esphome {
namespace esp_adf {

static const size_t BUFFER_COUNT = 25;

static const char *const TAG = "esp_adf.speaker";

void ESPADFSpeaker::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP ADF Speaker...");

  this->buffer_queue_ = xQueueCreate(BUFFER_COUNT, sizeof(DataEvent));
  this->event_queue_ = xQueueCreate(20, sizeof(TaskEvent));

  ESP_LOGCONFIG(TAG, "BufferQ:%p EventQ:%p", this->buffer_queue_, this->event_queue_);
}

void ESPADFSpeaker::start() { this->state_ = speaker::STATE_STARTING; }
void ESPADFSpeaker::start_() {
  if (!this->parent_->try_lock()) {
    return;  // Waiting for another i2s component to return lock
  }

  xTaskCreate(ESPADFSpeaker::player_task, "speaker_task", 8192, (void *) this, 0, &this->player_task_handle_);
}

void ESPADFSpeaker::player_task(void *params) {
  ESPADFSpeaker *this_speaker = (ESPADFSpeaker *) params;

  TaskEvent event;
  event.type = TaskEventType::STARTING;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);

  audio_pipeline_cfg_t pipeline_cfg = {
      .rb_size = 8 * 1024,
  };
  audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

  i2s_stream_cfg_t i2s_cfg = {};
  memset(&i2s_cfg, 0, sizeof(i2s_cfg));
  i2s_cfg.type = AUDIO_STREAM_WRITER;
  i2s_cfg.i2s_port = I2S_NUM_0;
  i2s_cfg.use_alc = false;
  i2s_cfg.volume = 0;
  i2s_cfg.out_rb_size = I2S_STREAM_RINGBUFFER_SIZE;
  i2s_cfg.task_stack = I2S_STREAM_TASK_STACK;
  i2s_cfg.task_core = I2S_STREAM_TASK_CORE;
  i2s_cfg.task_prio = I2S_STREAM_TASK_PRIO;
  i2s_cfg.stack_in_ext = false;
  i2s_cfg.multi_out_num = 0;
  i2s_cfg.uninstall_drv = true;
  i2s_cfg.need_expand = false;
  i2s_cfg.expand_src_bits = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_cfg.buffer_len = I2S_STREAM_BUF_SIZE;
  i2s_cfg.i2s_config.mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_cfg.i2s_config.sample_rate = 16000;
  i2s_cfg.i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
  i2s_cfg.i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_cfg.i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM;
  i2s_cfg.i2s_config.dma_buf_count = 8;
  i2s_cfg.i2s_config.dma_buf_len = 1024;
  i2s_cfg.i2s_config.use_apll = false;
  i2s_cfg.i2s_config.tx_desc_auto_clear = true;
  i2s_cfg.i2s_config.fixed_mclk = 0;
  i2s_cfg.i2s_config.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT;

  audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_cfg);
  /*
    rsp_filter_cfg_t rsp_cfg = {};
    rsp_cfg.src_rate = 16000;
    rsp_cfg.src_ch = 1;
    rsp_cfg.src_bits = 16;
    rsp_cfg.dest_rate = 16000;
    rsp_cfg.dest_ch = 2;
    rsp_cfg.dest_bits = 16;
    rsp_cfg.mode = RESAMPLE_DECODE_MODE;
    rsp_cfg.max_indata_bytes = RSP_FILTER_BUFFER_BYTE;
    rsp_cfg.out_len_bytes = RSP_FILTER_BUFFER_BYTE;
    rsp_cfg.type = ESP_RESAMPLE_TYPE_AUTO;
    rsp_cfg.complexity = 2;
    rsp_cfg.down_ch_idx = 0;
    rsp_cfg.prefer_flag = ESP_RSP_PREFER_TYPE_SPEED;
    rsp_cfg.out_rb_size = RSP_FILTER_RINGBUFFER_SIZE;
    rsp_cfg.task_stack = RSP_FILTER_TASK_STACK;
    rsp_cfg.task_core = RSP_FILTER_TASK_CORE;
    rsp_cfg.task_prio = RSP_FILTER_TASK_PRIO;
    rsp_cfg.stack_in_ext = true;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
  */
  raw_stream_cfg_t raw_cfg = {
      .type = AUDIO_STREAM_WRITER,
      .out_rb_size = 8 * 1024,
  };
  audio_element_handle_t raw_write = raw_stream_init(&raw_cfg);

  audio_pipeline_register(pipeline, raw_write, "raw");
  //  audio_pipeline_register(pipeline, filter, "filter");
  audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

  const char *link_tag[3] = {
      "raw",
      // "filter",
      "i2s",
  };
  audio_pipeline_link(pipeline, &link_tag[0], 2);

  audio_pipeline_run(pipeline);

  DataEvent data_event;

  event.type = TaskEventType::STARTED;
  xQueueSend(this_speaker->event_queue_, &event, 0);

  uint32_t last_received = millis();

  while (true) {
    if (xQueueReceive(this_speaker->buffer_queue_, &data_event, 0) != pdTRUE) {
      if (millis() - last_received > 500) {
        // No audio for 500ms, stop
        break;
      } else {
        continue;
      }
    }
    if (data_event.stop) {
      // Stop signal from main thread
      while (xQueueReceive(this_speaker->buffer_queue_, &data_event, 0) == pdTRUE) {
        // Flush queue
      }
      break;
    }

    size_t remaining = data_event.len;
    size_t current = 0;
    if (remaining > 0)
      last_received = millis();

    while (remaining > 0) {
      int bytes_written = raw_stream_write(raw_write, (char *) data_event.data + current, remaining);
      if (bytes_written == ESP_FAIL) {
        event = {.type = TaskEventType::WARNING, .err = ESP_FAIL};
        xQueueSend(this_speaker->event_queue_, &event, 0);
        continue;
      }

      remaining -= bytes_written;
      current += bytes_written;
    }

    event.type = TaskEventType::RUNNING;
    xQueueSend(this_speaker->event_queue_, &event, 0);
  }

  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);

  event.type = TaskEventType::STOPPING;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);

  audio_pipeline_unregister(pipeline, i2s_stream_writer);
  //  audio_pipeline_unregister(pipeline, filter);
  audio_pipeline_unregister(pipeline, raw_write);

  audio_pipeline_deinit(pipeline);
  audio_element_deinit(i2s_stream_writer);
  //  audio_element_deinit(filter);
  audio_element_deinit(raw_write);

  event.type = TaskEventType::STOPPED;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);

  while (true) {
    delay(10);
  }
}

void ESPADFSpeaker::stop() {
  if (this->state_ == speaker::STATE_STOPPED)
    return;
  if (this->state_ == speaker::STATE_STARTING) {
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  this->state_ = speaker::STATE_STOPPING;
  DataEvent data;
  data.stop = true;
  xQueueSendToFront(this->buffer_queue_, &data, portMAX_DELAY);
}

void ESPADFSpeaker::watch_() {
  TaskEvent event;
  if (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
    switch (event.type) {
      case TaskEventType::STARTING:
      case TaskEventType::STOPPING:
        break;
      case TaskEventType::STARTED:
        this->state_ = speaker::STATE_RUNNING;
        break;
      case TaskEventType::RUNNING:
        this->status_clear_warning();
        break;
      case TaskEventType::STOPPED:
        this->parent_->unlock();
        this->state_ = speaker::STATE_STOPPED;
        vTaskDelete(this->player_task_handle_);
        this->player_task_handle_ = nullptr;
        break;
      case TaskEventType::WARNING:
        ESP_LOGW(TAG, "Error writing to pipeline: %s", esp_err_to_name(event.err));
        this->status_set_warning();
        break;
    }
  }
}

void ESPADFSpeaker::loop() {
  this->watch_();
  switch (this->state_) {
    case speaker::STATE_STARTING:
      this->start_();
      break;
    case speaker::STATE_RUNNING:
    case speaker::STATE_STOPPING:
    case speaker::STATE_STOPPED:
      break;
  }
}

size_t ESPADFSpeaker::play(const uint8_t *data, size_t length) {
  if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
    this->start();
  }
  size_t remaining = length;
  size_t index = 0;
  while (remaining > 0) {
    DataEvent event;
    event.stop = false;
    size_t to_send_length = std::min(remaining, BUFFER_SIZE);
    event.len = to_send_length;
    memcpy(event.data, data + index, to_send_length);
    if (xQueueSend(this->buffer_queue_, &event, 0) != pdTRUE) {
      return index;  // Queue full
    }
    remaining -= to_send_length;
    index += to_send_length;
  }
  return index;
}

bool ESPADFSpeaker::has_buffered_data() const { return uxQueueMessagesWaiting(this->buffer_queue_) > 0; }

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

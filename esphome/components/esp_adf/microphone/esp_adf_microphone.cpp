#include "esp_adf_microphone.h"

#ifdef USE_ESP_IDF

#include <driver/i2s.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <audio_hal.h>
#include <filter_resample.h>
#include <i2s_stream.h>
#include <raw_stream.h>
#include <recorder_sr.h>

namespace esphome {
namespace esp_adf {

static const char *const TAG = "esp_adf.microphone";

void ESPADFMicrophone::setup() {
  this->ring_buffer_ = rb_create(BUFFER_SIZE, sizeof(int16_t));
  if (this->ring_buffer_ == nullptr) {
    ESP_LOGW(TAG, "Could not allocate ring buffer.");
    this->mark_failed();
    return;
  }
  this->feed_event_queue_ = xQueueCreate(20, sizeof(TaskEvent));
  this->feed_command_queue_ = xQueueCreate(20, sizeof(CommandEvent));
  this->fetch_command_queue_ = xQueueCreate(20, sizeof(CommandEvent));

  afe_config_t cfg_afe = {
      .aec_init = false,
      .se_init = true,
      .vad_init = true,
      .wakenet_init = false,
      .voice_communication_init = false,
      .voice_communication_agc_init = false,
      .voice_communication_agc_gain = 15,
      .vad_mode = VAD_MODE_3,
      .wakenet_model_name = nullptr,
      .wakenet_mode = DET_MODE_2CH_90,
      .afe_mode = SR_MODE_HIGH_PERF,
      .afe_perferred_core = 1,
      .afe_perferred_priority = 5,
      .afe_ringbuf_size = 50,
      .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,
      .agc_mode = AFE_MN_PEAK_AGC_MODE_3,
      .pcm_config =
          {
              .total_ch_num = 1,
              .mic_num = 1,
              .ref_num = 0,
              .sample_rate = 16000,
          },
      .debug_init = false,
      .debug_hook = {{AFE_DEBUG_HOOK_MASE_TASK_IN, NULL}, {AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL}},
  };

  this->afe_data_ = this->afe_handle_->create_from_config(&cfg_afe);
  this->afe_chunk_size_ = this->afe_handle_->get_feed_chunksize(this->afe_data_);
}

void ESPADFMicrophone::start() {
  if (this->is_failed())
    return;
  if (this->state_ == microphone::STATE_STOPPING) {
    ESP_LOGW(TAG, "Microphone is stopping, cannot start.");
    return;
  }
  this->state_ = microphone::STATE_STARTING;
}
void ESPADFMicrophone::start_() {
  if (!this->parent_->try_lock()) {
    return;
  }
  this->state_ = microphone::STATE_RUNNING;

  xTaskCreate(ESPADFMicrophone::feed_task, "feed_task", 8192, (void *) this, 0, &this->feed_task_handle_);
  xTaskCreate(ESPADFMicrophone::fetch_task, "fetch_task", 8192, (void *) this, 0, &this->fetch_task_handle_);
}

void ESPADFMicrophone::feed_task(void *params) {
  ESPADFMicrophone *this_mic = (ESPADFMicrophone *) params;
  TaskEvent event;

  event.type = TaskEventType::STARTING;
  xQueueSend(this_mic->feed_event_queue_, &event, portMAX_DELAY);

  size_t buffer_size = this_mic->afe_chunk_size_ * sizeof(int16_t);

  ExternalRAMAllocator<int16_t> allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);
  int16_t *afe_buffer = allocator.allocate(this_mic->afe_chunk_size_);
  if (afe_buffer == nullptr) {
    event.type = TaskEventType::STOPPED;
    xQueueSend(this_mic->feed_event_queue_, &event, portMAX_DELAY);

    while (true) {
      delay(10);
    };
    return;
  }

  i2s_driver_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = 44100,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
      .dma_buf_count = 3,
      .dma_buf_len = 300,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  audio_pipeline_cfg_t pipeline_cfg = {
      .rb_size = 8 * 1024,
  };
  audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

  i2s_stream_cfg_t i2s_cfg = {
      .type = AUDIO_STREAM_READER,
      .i2s_config = i2s_config,
      .i2s_port = I2S_NUM_0,
      .use_alc = false,
      .volume = 0,
      .out_rb_size = I2S_STREAM_RINGBUFFER_SIZE,
      .task_stack = I2S_STREAM_TASK_STACK,
      .task_core = I2S_STREAM_TASK_CORE,
      .task_prio = I2S_STREAM_TASK_PRIO,
      .stack_in_ext = false,
      .multi_out_num = 0,
      .uninstall_drv = true,
      .need_expand = false,
      .expand_src_bits = I2S_BITS_PER_SAMPLE_16BIT,
  };
  audio_element_handle_t i2s_stream_reader = i2s_stream_init(&i2s_cfg);

  rsp_filter_cfg_t rsp_cfg = {
      .src_rate = 44100,
      .src_ch = 2,
      .dest_rate = 16000,
      .dest_bits = 16,
      .dest_ch = 1,
      .src_bits = 16,
      .mode = RESAMPLE_DECODE_MODE,
      .max_indata_bytes = RSP_FILTER_BUFFER_BYTE,
      .out_len_bytes = RSP_FILTER_BUFFER_BYTE,
      .type = ESP_RESAMPLE_TYPE_AUTO,
      .complexity = 2,
      .down_ch_idx = 0,
      .prefer_flag = ESP_RSP_PREFER_TYPE_SPEED,
      .out_rb_size = RSP_FILTER_RINGBUFFER_SIZE,
      .task_stack = RSP_FILTER_TASK_STACK,
      .task_core = RSP_FILTER_TASK_CORE,
      .task_prio = RSP_FILTER_TASK_PRIO,
      .stack_in_ext = true,
  };
  audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);

  raw_stream_cfg_t raw_cfg = {
      .type = AUDIO_STREAM_READER,
      .out_rb_size = 8 * 1024,
  };
  audio_element_handle_t raw_read = raw_stream_init(&raw_cfg);

  audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
  audio_pipeline_register(pipeline, filter, "filter");
  audio_pipeline_register(pipeline, raw_read, "raw");

  const char *link_tag[3] = {"i2s", "filter", "raw"};
  audio_pipeline_link(pipeline, &link_tag[0], 3);

  audio_pipeline_run(pipeline);

  event.type = TaskEventType::STARTED;
  xQueueSend(this_mic->feed_event_queue_, &event, portMAX_DELAY);

  CommandEvent command_event;
  size_t fill_count = 0;

  while (true) {
    if (xQueueReceive(this_mic->feed_command_queue_, &command_event, 0) == pdTRUE) {
      if (command_event.stop) {
        // Stop signal from main thread
        break;
      }
    }

    int bytes_read = raw_stream_read(raw_read, (char *) (afe_buffer + fill_count), buffer_size - fill_count);

    if (bytes_read == -2 || bytes_read == 0) {
      // No data in buffers to read.
      continue;
    } else if (bytes_read < 0) {
      event.type = TaskEventType::WARNING;
      event.err = bytes_read;
      xQueueSend(this_mic->feed_event_queue_, &event, 0);
      continue;
    }

    event.type = TaskEventType::RUNNING;
    event.err = ESP_OK;
    xQueueSend(this_mic->feed_event_queue_, &event, 0);

    fill_count += bytes_read;

    if (fill_count == buffer_size) {
      this_mic->afe_handle_->feed(this_mic->afe_data_, afe_buffer);
      fill_count -= buffer_size;
    }
  }

  allocator.deallocate(afe_buffer, this_mic->afe_chunk_size_);

  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);

  event.type = TaskEventType::STOPPING;
  xQueueSend(this_mic->feed_event_queue_, &event, portMAX_DELAY);

  audio_pipeline_unregister(pipeline, i2s_stream_reader);
  audio_pipeline_unregister(pipeline, filter);
  audio_pipeline_unregister(pipeline, raw_read);

  audio_pipeline_deinit(pipeline);
  audio_element_deinit(i2s_stream_reader);
  audio_element_deinit(filter);
  audio_element_deinit(raw_read);

  event.type = TaskEventType::STOPPED;
  xQueueSend(this_mic->feed_event_queue_, &event, portMAX_DELAY);

  while (true) {
    delay(10);
  }
}

void ESPADFMicrophone::fetch_task(void *params) {
  ESPADFMicrophone *this_mic = (ESPADFMicrophone *) params;

  CommandEvent event;

  while (true) {
    if (xQueueReceive(this_mic->fetch_command_queue_, &event, 0) == pdTRUE) {
      if (event.stop) {
        // Stop signal from main thread
        break;
      }
    }
    afe_fetch_result_t *result = this_mic->afe_handle_->fetch(this_mic->afe_data_);

    if (result == nullptr) {
      continue;
    }

    int available = rb_bytes_available(this_mic->ring_buffer_);
    if (available < result->data_size) {
      rb_read(this_mic->ring_buffer_, nullptr, result->data_size - available, 0);
    }
    rb_write(this_mic->ring_buffer_, (char *) result->data, result->data_size, 0);
  }

  while (true) {
    delay(10);
  }
}

void ESPADFMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;
  if (this->state_ == microphone::STATE_STARTING) {
    this->state_ = microphone::STATE_STOPPED;
    return;
  }
  this->state_ = microphone::STATE_STOPPING;
}

void ESPADFMicrophone::stop_() {
  ESP_LOGD(TAG, "Stopping microphone");
  CommandEvent command_event;
  command_event.stop = true;
  xQueueSendToFront(this->feed_command_queue_, &command_event, portMAX_DELAY);
  xQueueSendToFront(this->fetch_command_queue_, &command_event, portMAX_DELAY);
}

size_t ESPADFMicrophone::read(int16_t *buf, size_t len) {
  int bytes_read = rb_read(this->ring_buffer_, (char *) buf, len, 0);

  if (bytes_read == -2 || bytes_read == 0) {
    // No data in buffers to read.
    return 0;
  } else if (bytes_read < 0) {
    ESP_LOGW(TAG, "Error reading from I2S microphone");
    this->status_set_warning();
    return 0;
  }
  this->status_clear_warning();

  return bytes_read;
}

void ESPADFMicrophone::read_() {
  std::vector<int16_t> samples;
  samples.resize(BUFFER_SIZE);
  this->read(samples.data(), samples.size());

  this->data_callbacks_.call(samples);
}

void ESPADFMicrophone::watch_() {
  TaskEvent event;
  if (xQueueReceive(this->feed_event_queue_, &event, 0) == pdTRUE) {
    switch (event.type) {
      case TaskEventType::STARTING:
      case TaskEventType::STARTED:
      case TaskEventType::STOPPING:
        break;
      case TaskEventType::RUNNING:
        this->status_clear_warning();
        break;
      case TaskEventType::STOPPED:
        this->parent_->unlock();
        this->state_ = microphone::STATE_STOPPED;
        vTaskDelete(this->feed_task_handle_);
        vTaskDelete(this->fetch_task_handle_);
        this->feed_task_handle_ = nullptr;
        this->fetch_task_handle_ = nullptr;
        ESP_LOGD(TAG, "Microphone stopped");
        break;
      case TaskEventType::WARNING:
        ESP_LOGW(TAG, "Error writing to pipeline: %s", esp_err_to_name(event.err));
        this->status_set_warning();
        break;
    }
  }
}

void ESPADFMicrophone::loop() {
  switch (this->state_) {
    case microphone::STATE_STOPPED:
      break;
    case microphone::STATE_STARTING:
      this->start_();
      break;
    case microphone::STATE_RUNNING:
      if (this->data_callbacks_.size() > 0) {
        this->read_();
      }
      break;
    case microphone::STATE_STOPPING:
      this->stop_();
      break;
  }
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

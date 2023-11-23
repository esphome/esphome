#include "esp_adf_microphone.h"

#ifdef USE_ESP_IDF

#include <driver/i2s.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm_stream.h>
#include <audio_element.h>
#include <audio_hal.h>
#include <audio_pipeline.h>
#include <filter_resample.h>
#include <i2s_stream.h>
#include <raw_stream.h>
#include <recorder_sr.h>

#include <board.h>

namespace esphome {
namespace esp_adf {

static const char *const TAG = "esp_adf.microphone";

void ESPADFMicrophone::setup() {
  this->ring_buffer_ = rb_create(8000, sizeof(int16_t));
  if (this->ring_buffer_ == nullptr) {
    ESP_LOGW(TAG, "Could not allocate ring buffer.");
    this->mark_failed();
    return;
  }

  this->read_event_queue_ = xQueueCreate(20, sizeof(TaskEvent));
  this->read_command_queue_ = xQueueCreate(20, sizeof(CommandEvent));
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

  xTaskCreate(ESPADFMicrophone::read_task, "read_task", 8192, (void *) this, 0, &this->read_task_handle_);
}

void ESPADFMicrophone::read_task(void *params) {
  ESPADFMicrophone *this_mic = (ESPADFMicrophone *) params;
  TaskEvent event;

  ExternalRAMAllocator<int16_t> allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);
  int16_t *buffer = allocator.allocate(BUFFER_SIZE / sizeof(int16_t));
  if (buffer == nullptr) {
    event.type = TaskEventType::WARNING;
    event.err = ESP_ERR_NO_MEM;
    xQueueSend(this_mic->read_event_queue_, &event, portMAX_DELAY);

    event.type = TaskEventType::STOPPED;
    event.err = ESP_OK;
    xQueueSend(this_mic->read_event_queue_, &event, portMAX_DELAY);

    while (true) {
      delay(10);
    }
    return;
  }

  event.type = TaskEventType::STARTING;
  xQueueSend(this_mic->read_event_queue_, &event, portMAX_DELAY);

  audio_pipeline_cfg_t pipeline_cfg = {
      .rb_size = 8 * 1024,
  };
  audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);

  i2s_driver_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
      .dma_buf_count = 8,
      .dma_buf_len = 128,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  i2s_stream_cfg_t i2s_cfg = {
      .type = AUDIO_STREAM_READER,
      .i2s_config = i2s_config,
      .i2s_port = static_cast<i2s_port_t>(CODEC_ADC_I2S_PORT),
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
      .src_rate = 16000,
      .src_ch = 2,
      .dest_rate = 16000,
      .dest_bits = 16,
      .dest_ch = 1,
      .src_bits = I2S_BITS_PER_SAMPLE_16BIT,
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

  algorithm_stream_cfg_t algo_cfg = {
      .input_type = ALGORITHM_STREAM_INPUT_TYPE1,
      .task_stack = 10 * 1024,
      .task_prio = ALGORITHM_STREAM_TASK_PERIOD,
      .task_core = ALGORITHM_STREAM_PINNED_TO_CORE,
      .out_rb_size = ALGORITHM_STREAM_RINGBUFFER_SIZE,
      .stack_in_ext = true,
      .rec_linear_factor = 1,
      .ref_linear_factor = 1,
      .debug_input = false,
      .swap_ch = false,
      // .algo_mask = ALGORITHM_STREAM_USE_AGC,
      // .algo_mask = (ALGORITHM_STREAM_USE_AEC | ALGORITHM_STREAM_USE_AGC | ALGORITHM_STREAM_USE_NS),
      // .algo_mask = (ALGORITHM_STREAM_USE_AGC | ALGORITHM_STREAM_USE_NS),
      .algo_mask = (ALGORITHM_STREAM_USE_AEC | ALGORITHM_STREAM_USE_NS),
      // .algo_mask = (ALGORITHM_STREAM_USE_NS),
      .sample_rate = 16000,
      .mic_ch = 1,
      .agc_gain = 10,
      .aec_low_cost = false,
  };

  // audio_element_handle_t algo_stream = algo_stream_init(&algo_cfg);

  raw_stream_cfg_t raw_cfg = {
      .type = AUDIO_STREAM_READER,
      .out_rb_size = 8 * 1024,
  };
  audio_element_handle_t raw_read = raw_stream_init(&raw_cfg);

  audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
  audio_pipeline_register(pipeline, filter, "filter");
  // audio_pipeline_register(pipeline, algo_stream, "algo");
  audio_pipeline_register(pipeline, raw_read, "raw");

  const char *link_tag[4] = {
      "i2s",
      "filter",
      // "algo",
      "raw",
  };
  audio_pipeline_link(pipeline, &link_tag[0], 3);

  audio_pipeline_run(pipeline);

  event.type = TaskEventType::STARTED;
  xQueueSend(this_mic->read_event_queue_, &event, portMAX_DELAY);

  CommandEvent command_event;

  while (true) {
    if (xQueueReceive(this_mic->read_command_queue_, &command_event, 0) == pdTRUE) {
      if (command_event.stop) {
        // Stop signal from main thread
        break;
      }
    }

    int bytes_read = raw_stream_read(raw_read, (char *) buffer, BUFFER_SIZE);

    if (bytes_read == -2 || bytes_read == 0) {
      // No data in buffers to read.
      continue;
    } else if (bytes_read < 0) {
      event.type = TaskEventType::WARNING;
      event.err = bytes_read;
      xQueueSend(this_mic->read_event_queue_, &event, 0);
      continue;
    }

    event.type = TaskEventType::RUNNING;
    event.err = bytes_read;
    xQueueSend(this_mic->read_event_queue_, &event, 0);

    int available = rb_bytes_available(this_mic->ring_buffer_);
    if (available < bytes_read) {
      rb_read(this_mic->ring_buffer_, nullptr, bytes_read - available, 0);
    }
    rb_write(this_mic->ring_buffer_, (char *) buffer, bytes_read, 0);
  }

  allocator.deallocate(buffer, BUFFER_SIZE / sizeof(int16_t));

  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);

  event.type = TaskEventType::STOPPING;
  xQueueSend(this_mic->read_event_queue_, &event, portMAX_DELAY);

  audio_pipeline_unregister(pipeline, i2s_stream_reader);
  audio_pipeline_unregister(pipeline, filter);
  // audio_pipeline_unregister(pipeline, algo_stream);
  audio_pipeline_unregister(pipeline, raw_read);

  audio_pipeline_deinit(pipeline);
  audio_element_deinit(i2s_stream_reader);
  audio_element_deinit(filter);
  // audio_element_deinit(algo_stream);
  audio_element_deinit(raw_read);

  event.type = TaskEventType::STOPPED;
  xQueueSend(this_mic->read_event_queue_, &event, portMAX_DELAY);

  while (true) {
    delay(10);
  }
}

void ESPADFMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->state_ == microphone::STATE_STOPPING || this->is_failed())
    return;
  this->state_ = microphone::STATE_STOPPING;
  CommandEvent command_event;
  command_event.stop = true;
  xQueueSendToFront(this->read_command_queue_, &command_event, portMAX_DELAY);
  ESP_LOGD(TAG, "Stopping microphone");
}

size_t ESPADFMicrophone::read(int16_t *buf, size_t len) {
  if (rb_bytes_available(this->ring_buffer_) == 0) {
    return 0;  // No data
  }
  int bytes_read = rb_read(this->ring_buffer_, (char *) buf, len, 0);

  if (bytes_read == -4 || bytes_read == -2 || bytes_read == 0) {
    // No data in buffers to read.
    return 0;
  } else if (bytes_read < 0) {
    ESP_LOGW(TAG, "Error reading from I2S microphone %s (%d)", esp_err_to_name(bytes_read), bytes_read);
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
  if (xQueueReceive(this->read_event_queue_, &event, 0) == pdTRUE) {
    switch (event.type) {
      case TaskEventType::STARTING:
      case TaskEventType::STOPPING:
        break;
      case TaskEventType::STARTED:
        ESP_LOGD(TAG, "Microphone started");
        this->state_ = microphone::STATE_RUNNING;
        break;
      case TaskEventType::RUNNING:
        this->status_clear_warning();
        // ESP_LOGD(TAG, "Putting %d bytes into ring buffer", event.err);
        break;
      case TaskEventType::STOPPED:
        this->parent_->unlock();
        this->state_ = microphone::STATE_STOPPED;
        vTaskDelete(this->read_task_handle_);
        this->read_task_handle_ = nullptr;
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
  this->watch_();
  switch (this->state_) {
    case microphone::STATE_STOPPED:
    case microphone::STATE_STOPPING:
      break;
    case microphone::STATE_STARTING:
      this->start_();
      break;
    case microphone::STATE_RUNNING:
      if (this->data_callbacks_.size() > 0) {
        this->read_();
      }
      break;
  }
}

}  // namespace esp_adf
}  // namespace esphome

#endif  // USE_ESP_IDF

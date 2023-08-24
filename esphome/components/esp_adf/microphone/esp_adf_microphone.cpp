#include "esp_adf_microphone.h"

#ifdef USE_ESP_IDF

#include <driver/i2s.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <audio_hal.h>
#include <filter_resample.h>
#include <i2s_stream.h>
#include <raw_stream.h>

namespace esphome {
namespace esp_adf {

static const size_t BUFFER_SIZE = 1024;

static const char *const TAG = "esp_adf.microphone";

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

  ESP_LOGI(TAG, "Init pipeline");
  audio_pipeline_cfg_t pipeline_cfg = {
      .rb_size = 8 * 1024,
  };
  this->pipeline_ = audio_pipeline_init(&pipeline_cfg);

  ESP_LOGI(TAG, "Init i2s stream");
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
  this->i2s_stream_reader_ = i2s_stream_init(&i2s_cfg);

  ESP_LOGI(TAG, "Init filter");
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
  this->filter_ = rsp_filter_init(&rsp_cfg);

  ESP_LOGI(TAG, "Init raw stream");
  raw_stream_cfg_t raw_cfg = {
      .type = AUDIO_STREAM_READER,
      .out_rb_size = 8 * 1024,
  };
  this->raw_read_ = raw_stream_init(&raw_cfg);

  ESP_LOGI(TAG, "Register all elements to audio pipeline");
  audio_pipeline_register(this->pipeline_, this->i2s_stream_reader_, "i2s");
  audio_pipeline_register(this->pipeline_, this->filter_, "filter");
  audio_pipeline_register(this->pipeline_, this->raw_read_, "raw");

  const char *link_tag[3] = {"i2s", "filter", "raw"};
  audio_pipeline_link(this->pipeline_, &link_tag[0], 3);

  ESP_LOGI(TAG, "Starting pipeline");
  audio_pipeline_run(this->pipeline_);

  this->state_ = microphone::STATE_RUNNING;
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
  audio_pipeline_stop(this->pipeline_);
  audio_pipeline_wait_for_stop(this->pipeline_);
  audio_pipeline_terminate(this->pipeline_);

  audio_pipeline_unregister(this->pipeline_, this->i2s_stream_reader_);
  audio_pipeline_unregister(this->pipeline_, this->filter_);
  audio_pipeline_unregister(this->pipeline_, this->raw_read_);

  audio_pipeline_deinit(this->pipeline_);
  audio_element_deinit(this->i2s_stream_reader_);
  audio_element_deinit(this->filter_);
  audio_element_deinit(this->raw_read_);

  this->parent_->unlock();
  this->state_ = microphone::STATE_STOPPED;
  ESP_LOGD(TAG, "Microphone stopped");
}

size_t ESPADFMicrophone::read(int16_t *buf, size_t len) {
  int bytes_read = raw_stream_read(this->raw_read_, (char *) buf, len);

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

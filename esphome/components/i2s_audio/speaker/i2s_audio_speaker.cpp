#include "i2s_audio_speaker.h"

#ifdef USE_ESP32

#include <driver/i2s.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace i2s_audio {

static const size_t BUFFER_COUNT = 20;

static const char *const TAG = "i2s_audio.speaker";

void I2SAudioSpeaker::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2S Audio Speaker...");

  this->buffer_queue_ = xQueueCreate(BUFFER_COUNT, sizeof(DataEvent));
  if (this->buffer_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create buffer queue");
    this->mark_failed();
    return;
  }

  this->event_queue_ = xQueueCreate(BUFFER_COUNT, sizeof(TaskEvent));
  if (this->event_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event queue");
    this->mark_failed();
    return;
  }
}

void I2SAudioSpeaker::start() {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot start audio, speaker failed to setup");
    return;
  }
  if (this->task_created_) {
    ESP_LOGW(TAG, "Called start while task has been already created.");
    return;
  }
  this->state_ = speaker::STATE_STARTING;
}
void I2SAudioSpeaker::start_() {
  if (this->task_created_) {
    return;
  }
  if (!this->parent_->try_lock()) {
    return;  // Waiting for another i2s component to return lock
  }

  xTaskCreate(I2SAudioSpeaker::player_task, "speaker_task", 8192, (void *) this, 1, &this->player_task_handle_);
  this->task_created_ = true;
}

template<typename a, typename b> const uint8_t *convert_data_format(const a *from, b *to, size_t &bytes, bool repeat) {
  if (sizeof(a) == sizeof(b) && !repeat) {
    return reinterpret_cast<const uint8_t *>(from);
  }
  const b *result = to;
  for (size_t i = 0; i < bytes; i += sizeof(a)) {
    b value = static_cast<b>(*from++) << (sizeof(b) - sizeof(a)) * 8;
    *to++ = value;
    if (repeat)
      *to++ = value;
  }
  bytes *= (sizeof(b) / sizeof(a)) * (repeat ? 2 : 1);  // NOLINT
  return reinterpret_cast<const uint8_t *>(result);
}

void I2SAudioSpeaker::player_task(void *params) {
  I2SAudioSpeaker *this_speaker = (I2SAudioSpeaker *) params;

  TaskEvent event;
  event.type = TaskEventType::STARTING;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);

  i2s_driver_config_t config = {
      .mode = (i2s_mode_t) (this_speaker->i2s_mode_ | I2S_MODE_TX),
      .sample_rate = this_speaker->sample_rate_,
      .bits_per_sample = this_speaker->bits_per_sample_,
      .channel_format = this_speaker->channel_,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = this_speaker->use_apll_,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = this_speaker->bits_per_channel_,
  };
#if SOC_I2S_SUPPORTS_DAC
  if (this_speaker->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    config.mode = (i2s_mode_t) (config.mode | I2S_MODE_DAC_BUILT_IN);
  }
#endif

  esp_err_t err = i2s_driver_install(this_speaker->parent_->get_port(), &config, 0, nullptr);
  if (err != ESP_OK) {
    event.type = TaskEventType::WARNING;
    event.err = err;
    xQueueSend(this_speaker->event_queue_, &event, 0);
    event.type = TaskEventType::STOPPED;
    xQueueSend(this_speaker->event_queue_, &event, 0);
    while (true) {
      delay(10);
    }
  }

#if SOC_I2S_SUPPORTS_DAC
  if (this_speaker->internal_dac_mode_ == I2S_DAC_CHANNEL_DISABLE) {
#endif
    i2s_pin_config_t pin_config = this_speaker->parent_->get_pin_config();
    pin_config.data_out_num = this_speaker->dout_pin_;

    i2s_set_pin(this_speaker->parent_->get_port(), &pin_config);
#if SOC_I2S_SUPPORTS_DAC
  } else {
    i2s_set_dac_mode(this_speaker->internal_dac_mode_);
  }
#endif

  DataEvent data_event;

  event.type = TaskEventType::STARTED;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);

  int32_t buffer[BUFFER_SIZE];

  while (true) {
    if (xQueueReceive(this_speaker->buffer_queue_, &data_event, this_speaker->timeout_ / portTICK_PERIOD_MS) !=
        pdTRUE) {
      break;  // End of audio from main thread
    }
    if (data_event.stop) {
      // Stop signal from main thread
      xQueueReset(this_speaker->buffer_queue_);  // Flush queue
      break;
    }

    const uint8_t *data = data_event.data;
    size_t remaining = data_event.len;
    switch (this_speaker->bits_per_sample_) {
      case I2S_BITS_PER_SAMPLE_8BIT:
      case I2S_BITS_PER_SAMPLE_16BIT: {
        data = convert_data_format(reinterpret_cast<const int16_t *>(data), reinterpret_cast<int16_t *>(buffer),
                                   remaining, this_speaker->channel_ == I2S_CHANNEL_FMT_ALL_LEFT);
        break;
      }
      case I2S_BITS_PER_SAMPLE_24BIT:
      case I2S_BITS_PER_SAMPLE_32BIT: {
        data = convert_data_format(reinterpret_cast<const int16_t *>(data), reinterpret_cast<int32_t *>(buffer),
                                   remaining, this_speaker->channel_ == I2S_CHANNEL_FMT_ALL_LEFT);
        break;
      }
    }

    while (remaining != 0) {
      size_t bytes_written;
      esp_err_t err =
          i2s_write(this_speaker->parent_->get_port(), data, remaining, &bytes_written, (32 / portTICK_PERIOD_MS));
      if (err != ESP_OK) {
        event = {.type = TaskEventType::WARNING, .err = err};
        if (xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS) != pdTRUE) {
          ESP_LOGW(TAG, "Failed to send WARNING event");
        }
        continue;
      }
      data += bytes_written;
      remaining -= bytes_written;
    }
  }

  event.type = TaskEventType::STOPPING;
  if (xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS) != pdTRUE) {
    ESP_LOGW(TAG, "Failed to send STOPPING event");
  }

  i2s_zero_dma_buffer(this_speaker->parent_->get_port());

  i2s_driver_uninstall(this_speaker->parent_->get_port());

  event.type = TaskEventType::STOPPED;
  if (xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS) != pdTRUE) {
    ESP_LOGW(TAG, "Failed to send STOPPED event");
  }

  while (true) {
    delay(10);
  }
}

void I2SAudioSpeaker::stop() { this->stop_(false); }

void I2SAudioSpeaker::finish() { this->stop_(true); }

void I2SAudioSpeaker::stop_(bool wait_on_empty) {
  if (this->is_failed())
    return;
  if (this->state_ == speaker::STATE_STOPPED)
    return;
  if (this->state_ == speaker::STATE_STARTING) {
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  this->state_ = speaker::STATE_STOPPING;
  DataEvent data;
  data.stop = true;
  if (wait_on_empty) {
    xQueueSend(this->buffer_queue_, &data, portMAX_DELAY);
  } else {
    xQueueSendToFront(this->buffer_queue_, &data, portMAX_DELAY);
  }
}

void I2SAudioSpeaker::watch_() {
  TaskEvent event;
  if (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
    switch (event.type) {
      case TaskEventType::STARTING:
        ESP_LOGD(TAG, "Starting I2S Audio Speaker");
        break;
      case TaskEventType::STARTED:
        ESP_LOGD(TAG, "Started I2S Audio Speaker");
        this->state_ = speaker::STATE_RUNNING;
        this->status_clear_warning();
        break;
      case TaskEventType::STOPPING:
        ESP_LOGD(TAG, "Stopping I2S Audio Speaker");
        break;
      case TaskEventType::STOPPED:
        this->state_ = speaker::STATE_STOPPED;
        vTaskDelete(this->player_task_handle_);
        this->task_created_ = false;
        this->player_task_handle_ = nullptr;
        this->parent_->unlock();
        xQueueReset(this->buffer_queue_);
        ESP_LOGD(TAG, "Stopped I2S Audio Speaker");
        break;
      case TaskEventType::WARNING:
        ESP_LOGW(TAG, "Error writing to I2S: %s", esp_err_to_name(event.err));
        this->status_set_warning();
        break;
    }
  }
}

void I2SAudioSpeaker::loop() {
  switch (this->state_) {
    case speaker::STATE_STARTING:
      this->start_();
      [[fallthrough]];
    case speaker::STATE_RUNNING:
    case speaker::STATE_STOPPING:
      this->watch_();
      break;
    case speaker::STATE_STOPPED:
      break;
  }
}

size_t I2SAudioSpeaker::play(const uint8_t *data, size_t length) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot play audio, speaker failed to setup");
    return 0;
  }
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
      return index;
    }
    remaining -= to_send_length;
    index += to_send_length;
  }
  return index;
}

bool I2SAudioSpeaker::has_buffered_data() const { return uxQueueMessagesWaiting(this->buffer_queue_) > 0; }

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32

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

  this->buffer_queue_ = RingBuffer::create(BUFFER_COUNT * BUFFER_SIZE * sizeof(int16_t));
  if (this->buffer_queue_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate ring buffer");
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
  this->set_state_(speaker::STATE_STARTING);
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

void I2SAudioSpeaker::player_task(void *params) {
  I2SAudioSpeaker *this_speaker = (I2SAudioSpeaker *) params;

  TaskEvent event;
  event.type = TaskEventType::STARTING;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);

  i2s_driver_config_t config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 128,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = I2S_PIN_NO_CHANGE,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
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

  event.type = TaskEventType::STARTED;
  xQueueSend(this_speaker->event_queue_, &event, portMAX_DELAY);
  bool last_available_state = false;
  const uint8_t wordsize = this_speaker->use_16bit_mode_ ? 2 : 4;
  while (true) {
    uint32_t sample;
    uint8_t error_count = 0;
    size_t bytes_written = 0;
    while (this_speaker->buffer_queue_->available() > 0) {
      if (!last_available_state) {
        ESP_LOGV(TAG, "PLAYING");
        event.type = TaskEventType::PLAYING;
        event.err = 0;
        if (xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS) != pdTRUE) {
          ESP_LOGW(TAG, "Failed to send PLAYING event");
        }
        last_available_state = true;
      }
      this_speaker->buffer_queue_->read((void *) &sample, wordsize);
      if (!this_speaker->use_16bit_mode_) {
        sample = (sample << 16) | (sample & 0xFFFF);
      }
      esp_err_t err =
          i2s_write(this_speaker->parent_->get_port(), &sample, wordsize, &bytes_written, (10 / portTICK_PERIOD_MS));
      if (err != ESP_OK) {
        event = {.type = TaskEventType::WARNING, .err = err};
        error_count++;
        if (xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS) != pdTRUE) {
          ESP_LOGW(TAG, "Failed to send WARNING event");
        }
      } else if (bytes_written != wordsize) {
        event = {.type = TaskEventType::WARNING, .err = ESP_FAIL};
        error_count++;
        if (xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS) != pdTRUE) {
          ESP_LOGW(TAG, "Failed to send WARNING event");
        }
      } else {
        error_count = 0;
      }
      if (error_count >= 5) {
        break;
      }
    }
    if (this_speaker->state_ == speaker::STATE_STOPPING || error_count >= 5) {
      ESP_LOGV(TAG, "STOPPING");
      break;
    }
    if (last_available_state) {
      ESP_LOGV(TAG, "PAUSING");
      event.type = TaskEventType::PAUSING;
      event.err = 0;
      if (xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send PLAYING event");
      }
      last_available_state = false;
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

void I2SAudioSpeaker::finish() {
  ESP_LOGE(TAG, "Finishing I2S Audio Speaker");
  if (this->is_failed())
    return;
  if (this->state_ == speaker::STATE_STOPPED)
    return;
  if (this->state_ == speaker::STATE_STARTING) {
    this->set_state_(speaker::STATE_STOPPED);
    return;
  }
  this->set_state_(speaker::STATE_STOPPING);
}

void I2SAudioSpeaker::stop() {
  this->finish();
  this->buffer_queue_->reset();
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
        this->set_state_(speaker::STATE_RUNNING);
        break;
      case TaskEventType::STOPPING:
        ESP_LOGD(TAG, "Stopping I2S Audio Speaker");
        break;
      case TaskEventType::PLAYING:
      case TaskEventType::PAUSING:
        this->status_clear_warning();
        break;
      case TaskEventType::STOPPED:
        this->set_state_(speaker::STATE_STOPPED);
        vTaskDelete(this->player_task_handle_);
        this->task_created_ = false;
        this->player_task_handle_ = nullptr;
        this->parent_->unlock();
        this->buffer_queue_->reset();
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
  if (this->state_ != speaker::STATE_STOPPED) {
    if (this->state_ == speaker::STATE_STARTING) {
      this->start_();
    }
    this->watch_();
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
  length = std::min(this->buffer_queue_->free(), length);
  this->buffer_queue_->write((void *) data, length);

  return length;
}

bool I2SAudioSpeaker::has_buffered_data() const { return this->buffer_queue_->available() > 0; }

size_t I2SAudioSpeaker::available_space() const { return this->buffer_queue_->free(); }

static const LogString *state_to_string(speaker::State state) {
  switch (state) {
    case speaker::STATE_STOPPED:
      return LOG_STR("STATE_STOPPED");
    case speaker::STATE_STARTING:
      return LOG_STR("STATE_STARTING");
    case speaker::STATE_RUNNING:
      return LOG_STR("STATE_RUNNING");
    case speaker::STATE_STOPPING:
      return LOG_STR("STATE_STOPPING");

    default:
      return LOG_STR("UNKNOWN");
  }
};

void I2SAudioSpeaker::set_state_(speaker::State state) {
  speaker::State old_state = this->state_;
  this->state_ = state;
  ESP_LOGD(TAG, "State changed from %s to %s", LOG_STR_ARG(state_to_string(old_state)),
           LOG_STR_ARG(state_to_string(state)));
}

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32

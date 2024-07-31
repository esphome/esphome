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
  const uint8_t wordsize = this->use_16bit_mode_ ? 2 : 4;
  this->buffer_queue_ = xStreamBufferCreate(BUFFER_COUNT * BUFFER_SIZE * wordsize, wordsize);
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
  xTaskCreate(I2SAudioSpeaker::player_task, "speaker_task", 8192, (void *) this, 1, &this->player_task_handle_);
  vTaskSuspend(this->player_task_handle_);
}

void I2SAudioSpeaker::flush() {
  if (this->buffer_queue_ != nullptr) {
    xStreamBufferReset(this->buffer_queue_);
  }
}

void I2SAudioSpeaker::start() {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot start audio, speaker failed to setup");
    return;
  }
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Speaker has not yet runned setup.");
    return;
  }
  if (eRunning == eTaskGetState(this->player_task_handle_)) {
    ESP_LOGE(TAG, "Called start while task is already running.");
    return;
  }
  this->set_state_(speaker::STATE_STARTING);
}

void I2SAudioSpeaker::start_() {
  if (!this->parent_->try_lock()) {
    return;  // Waiting for another i2s component to return lock
  }

  i2s_port_t port = this->parent_->get_port();

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
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    config.mode = (i2s_mode_t) (config.mode | I2S_MODE_DAC_BUILT_IN);
  }
#endif

  esp_err_t err = i2s_driver_install(port, &config, 0, nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize I2S driver: %s", esp_err_to_name(err));
    this->mark_failed();
    this->set_state_(speaker::STATE_STOPPED);
    return;
  }

#if SOC_I2S_SUPPORTS_DAC
  if (this->internal_dac_mode_ == I2S_DAC_CHANNEL_DISABLE) {
#endif
    i2s_pin_config_t pin_config = this->parent_->get_pin_config();
    pin_config.data_out_num = this->dout_pin_;

    i2s_set_pin(port, &pin_config);
#if SOC_I2S_SUPPORTS_DAC
  } else {
    i2s_set_dac_mode(this->internal_dac_mode_);
  }
#endif
  vTaskResume(this->player_task_handle_);
  this->set_state_(speaker::STATE_RUNNING);
}

size_t I2SAudioSpeaker::play(const uint8_t *data, size_t length) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot play audio, speaker failed to setup");
    return 0;
  }
  if (this->state_ != speaker::STATE_RUNNING) {
    this->start();
  }

  length = std::min(this->available_space(), length);
  return xStreamBufferSend(this->buffer_queue_, data, length, 0);
}

void I2SAudioSpeaker::player_task(void *params) {
  I2SAudioSpeaker *this_speaker = (I2SAudioSpeaker *) params;
  bool is_playing = false;
  const uint8_t wordsize = this_speaker->use_16bit_mode_ ? 2 : 4;
  TaskEvent event;
  uint8_t error_count = 0;
  uint32_t sample;
  size_t bytes_written;

  while (true) {
    if (this_speaker->buffer_queue_ != nullptr) {
      int ret = xStreamBufferReceive(this_speaker->buffer_queue_, &sample, wordsize, portMAX_DELAY);
      if (ret == wordsize) {
        if (!is_playing) {
          event.type = TaskEventType::PLAYING;
          xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS);
          is_playing = true;
        }

        if (!this_speaker->use_16bit_mode_) {
          sample = (sample << 16) | (sample & 0xFFFF);
        }
        esp_err_t err = i2s_write(this_speaker->parent_->get_port(), &sample, wordsize, &bytes_written, portMAX_DELAY);
        if (err != ESP_OK) {
          event.type = TaskEventType::WARNING;
          event.err = err;
          event.stopped = ++error_count >= 5;
          xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS);
        } else if (bytes_written != wordsize) {
          event.type = TaskEventType::WARNING;
          event.err = ESP_FAIL;
          event.data = bytes_written;
          event.stopped = ++error_count >= 5;
          xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS);
        } else {
          error_count = 0;
        }
      } else if (ret == 0) {
        if (!is_playing) {
          event.type = TaskEventType::PAUSING;
          xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS);
          is_playing = true;
        }

        vTaskDelay(1);
      } else {
        event.type = TaskEventType::WARNING;
        event.err = ESP_FAIL;
        event.data = -ret;

        event.stopped = ++error_count >= 5;
        xQueueSend(this_speaker->event_queue_, &event, 10 / portTICK_PERIOD_MS);
      }
    }
  }
}

void I2SAudioSpeaker::finish() {
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
  this->flush();
}

void I2SAudioSpeaker::stop_() {
  if (this->has_buffered_data()) {
    return;
  }

  vTaskSuspend(this->player_task_handle_);
  // make sure the speaker has no voltage on the pins before closing the I2S poort
  size_t bytes_written;
  uint32_t sample = 0;
  i2s_write(this->parent_->get_port(), &sample, 4, &bytes_written, (10 / portTICK_PERIOD_MS));

  i2s_zero_dma_buffer(this->parent_->get_port());
  i2s_driver_uninstall(this->parent_->get_port());

  this->parent_->unlock();
  this->set_state_(speaker::STATE_STOPPED);
}

void I2SAudioSpeaker::loop() {
  TaskEvent event;
  switch (this->state_) {
    case speaker::STATE_STOPPED:
      /* code */
      return;
    case speaker::STATE_RUNNING:

      if (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
        switch (event.type) {
          case TaskEventType::PLAYING:
            ESP_LOGI(TAG, "PLAYING");
            this->status_clear_warning();
            break;
          case TaskEventType::PAUSING:
            ESP_LOGW(TAG, "PAUSING");
            break;
          case TaskEventType::WARNING:
            if (event.data < 0) {
              ESP_LOGW(TAG, "Read data size mismatch: %d instead of %d", -event.data, wordsize_());

            } else if (event.data > 0) {
              ESP_LOGW(TAG, "Write data size mismatch: %d instead of %d", event.data, wordsize_());

            } else {
              ESP_LOGW(TAG, "Error writing to I2S: %s", esp_err_to_name(event.err));
            }
            this->status_set_warning();
            break;
        }
      }
      break;
    case speaker::STATE_STARTING:
      this->start_();
      return;
    case speaker::STATE_STOPPING:
      this->stop_();
      return;
  }
}

bool I2SAudioSpeaker::has_buffered_data() const {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot play audio, speaker failed to setup");
    return false;
  }
  return xStreamBufferBytesAvailable(this->buffer_queue_) > 0;
}

size_t I2SAudioSpeaker::available_space() const {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot play audio, speaker failed to setup");
    return 0;
  }
  return xStreamBufferSpacesAvailable(this->buffer_queue_);
}

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
  ESP_LOGV(TAG, "State changed from %s to %s", LOG_STR_ARG(state_to_string(old_state)),
           LOG_STR_ARG(state_to_string(state)));
}

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32

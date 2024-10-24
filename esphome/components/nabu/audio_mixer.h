#pragma once

#ifdef USE_ESP_IDF

#include "esphome/components/media_player/media_player.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace nabu {

// Mixes two incoming audio streams together
//  - The media stream intended for music playback
//    - Able to duck (made quieter)
//    - Able to pause
//  - The announcement stream is intended for TTS reponses or various beeps/sound effects
//    - Unable to duck
//    - Unable to pause
//  - Each stream has a corresponding input ring buffer. Retrieved via the `get_media_ring_buffer` and
//    `get_announcement_ring_buffer` functions
//  - The mixed audio is sent to the configured speaker component.
//  - The mixer runs as a FreeRTOS task
//    - The task reports its state using the TaskEvent queue. Regularly call the  `read_event` function to obtain the
//      current state
//    - Commands are sent to the task using a the CommandEvent queue. Use the `send_command` function to do so.
//    - Use the `start` function to initiate. The `stop` function deletes the task, but be sure to send a STOP command
//      first to avoid memory leaks.

enum class EventType : uint8_t {
  STARTING = 0,
  STARTED,
  RUNNING,
  IDLE,
  STOPPING,
  STOPPED,
  WARNING = 255,
};

// Used for reporting the state of the mixer task
struct TaskEvent {
  EventType type;
  esp_err_t err;
};

enum class CommandEventType : uint8_t {
  STOP,                // Stop mixing to prepare for stopping the mixing task
  DUCK,                // Duck the media audio
  PAUSE_MEDIA,         // Pauses the media stream
  RESUME_MEDIA,        // Resumes the media stream
  CLEAR_MEDIA,         // Resets the media ring buffer
  CLEAR_ANNOUNCEMENT,  // Resets the announcement ring buffer
};

// Used to send commands to the mixer task
struct CommandEvent {
  CommandEventType command;
  uint8_t decibel_reduction;
  size_t transition_samples = 0;
};

// Gives the Q15 fixed point scaling factor to reduce by 0 dB, 1dB, ..., 50 dB
// dB to PCM scaling factor formula: floating_point_scale_factor = 2^(-db/6.014)
// float to Q15 fixed point formula: q15_scale_factor = floating_point_scale_factor * 2^(15)
static const std::vector<int16_t> decibel_reduction_table = {
    32767, 29201, 26022, 23189, 20665, 18415, 16410, 14624, 13032, 11613, 10349, 9222, 8218, 7324, 6527, 5816, 5183,
    4619,  4116,  3668,  3269,  2913,  2596,  2313,  2061,  1837,  1637,  1459,  1300, 1158, 1032, 920,  820,  731,
    651,   580,   517,   461,   411,   366,   326,   291,   259,   231,   206,   183,  163,  146,  130,  116,  103};

class AudioMixer {
 public:
  /// @brief Sends a CommandEvent to the command queue
  /// @param command Pointer to CommandEvent object to be sent
  /// @param ticks_to_wait The number of FreeRTOS ticks to wait for an event to appear on the queue. Defaults to 0.
  /// @return pdTRUE if successful, pdFALSE otherwises
  BaseType_t send_command(CommandEvent *command, TickType_t ticks_to_wait = portMAX_DELAY) {
    return xQueueSend(this->command_queue_, command, ticks_to_wait);
  }

  /// @brief Reads a TaskEvent from the event queue indicating its current status
  /// @param event Pointer to TaskEvent object to store the event in
  /// @param ticks_to_wait The number of FreeRTOS ticks to wait for an event to appear on the queue. Defaults to 0.
  /// @return pdTRUE if successful, pdFALSE otherwise
  BaseType_t read_event(TaskEvent *event, TickType_t ticks_to_wait = 0) {
    return xQueueReceive(this->event_queue_, event, ticks_to_wait);
  }

  /// @brief Starts the mixer task
  /// @param speaker Pointer to Speaker component
  /// @param task_name FreeRTOS task name
  /// @param priority FreeRTOS task priority. Defaults to 1
  /// @return ESP_OK if successful, and error otherwise
  esp_err_t start(speaker::Speaker *speaker, const std::string &task_name, UBaseType_t priority = 1);

  /// @brief Stops the mixer task and clears the queues
  void stop();

  /// @brief Retrieves the media stream's ring buffer pointer
  /// @return pointer to media ring buffer
  RingBuffer *get_media_ring_buffer() { return this->media_ring_buffer_.get(); }

  /// @brief Retrieves the announcement stream's ring buffer pointer
  /// @return pointer to announcement ring buffer
  RingBuffer *get_announcement_ring_buffer() { return this->announcement_ring_buffer_.get(); }

  /// @brief Suspends the mixer task
  void suspend_task();
  /// @brief Resumes the mixer task
  void resume_task();

 protected:
  /// @brief Allocates the ring buffers, task stack, and queues
  /// @return ESP_OK if successful or an error otherwise
  esp_err_t allocate_buffers_();

  /// @brief Resets the media and anouncement ring buffers
  void reset_ring_buffers_();

  /// @brief Mixes the media and announcement samples. If the resulting audio clips, the media samples are first scaled.
  /// @param media_buffer buffer for media samples
  /// @param announcement_buffer buffer for announcement samples
  /// @param combination_buffer buffer for the mixed samples
  /// @param samples_to_mix number of samples in the media and annoucnement buffers to mix together
  void mix_audio_samples_without_clipping_(int16_t *media_buffer, int16_t *announcement_buffer,
                                           int16_t *combination_buffer, size_t samples_to_mix);

  /// @brief Scales audio samples. Scales in place when audio_samples == output_buffer.
  /// @param audio_samples PCM int16 audio samples
  /// @param output_buffer Buffer to store the scaled samples
  /// @param scale_factor Q15 fixed point scaling factor
  /// @param samples_to_scale Number of samples to scale
  void scale_audio_samples_(int16_t *audio_samples, int16_t *output_buffer, int16_t scale_factor,
                            size_t samples_to_scale);

  static void audio_mixer_task_(void *params);
  TaskHandle_t task_handle_{nullptr};
  StaticTask_t task_stack_;
  StackType_t *stack_buffer_{nullptr};

  // Reports events from the mixer task
  QueueHandle_t event_queue_;

  // Stores commands to send the mixer task
  QueueHandle_t command_queue_;

  speaker::Speaker *speaker_{nullptr};

  std::unique_ptr<RingBuffer> media_ring_buffer_;
  std::unique_ptr<RingBuffer> announcement_ring_buffer_;
};
}  // namespace nabu
}  // namespace esphome

#endif

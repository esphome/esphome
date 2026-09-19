#pragma once

#ifdef USE_ESP32

#include "preprocessor_settings.h"
#include "streaming_model.h"

#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/static_task.h"

#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

#include <freertos/event_groups.h>

#include <frontend.h>
#include <frontend_util.h>

namespace esphome::micro_wake_word {

enum State {
  STARTING,
  DETECTING_WAKE_WORD,
  STOPPING,
  STOPPED,
};

class MicroWakeWord final : public Component
#ifdef USE_OTA_STATE_LISTENER
    ,
                            public ota::OTAGlobalStateListener
#endif
{
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

#ifdef USE_OTA_STATE_LISTENER
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) override;
#endif

  void start();
  void stop();

  bool is_running() const { return this->state_ != State::STOPPED; }

  void set_features_step_size(uint8_t step_size) { this->features_step_size_ = step_size; }

  void set_microphone_source(microphone::MicrophoneSource *microphone_source) {
    this->microphone_source_ = microphone_source;
  }

  void set_stop_after_detection(bool stop_after_detection) { this->stop_after_detection_ = stop_after_detection; }

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  Trigger<std::string> *get_wake_word_detected_trigger() { return &this->wake_word_detected_trigger_; }

  void add_wake_word_model(WakeWordModel *model);

  /// @brief Adds a runtime-downloaded wake word model. Must be called from the main loop.
  /// If the inference task is running it is paused at a safe point before the model lists are mutated,
  /// so the task never observes a half-updated vector.
  /// Callers should check get_model_by_id() before constructing the model: a WakeWordModel permanently
  /// claims a preference backend that is not released when the model is destroyed, so building one only to
  /// have it rejected here costs internal RAM that never comes back.
  /// @return True if the model was added, false if it has no valid data, on a duplicate id, or if the task
  /// could not be paused
  bool add_runtime_model(std::unique_ptr<WakeWordModel> model);

  /// @brief Removes a runtime-downloaded wake word model and frees its interpreter, arenas, and model buffer.
  /// Must be called from the main loop. If the inference task is running it is paused at a safe point first,
  /// and any queued detection events are dropped (they hold pointers into the model being destroyed).
  /// @return True if the model was removed, false if the id is not a runtime model or the task could not be paused
  bool remove_runtime_model(const std::string &model_id);

  /// @brief Returns the wake word model with the given id, or nullptr if none matches (compiled or runtime).
  /// Must be called from the main loop, as the returned pointer is invalidated by remove_runtime_model().
  WakeWordModel *get_model_by_id(const std::string &model_id);

  /// @brief Returns the ids of all runtime-downloaded models. Must be called from the main loop.
  std::vector<std::string> get_runtime_model_ids();

  /// @brief Returns the feature step size (ms) the frontend is configured for. Runtime models must match it.
  uint8_t get_features_step_size() const { return this->features_step_size_; }

#ifdef USE_MICRO_WAKE_WORD_VAD
  void add_vad_model(const uint8_t *model_start, uint8_t probability_cutoff, size_t sliding_window_size,
                     size_t tensor_arena_size);

  // Intended for the voice assistant component to fetch VAD status
  bool get_vad_state() { return this->vad_state_; }
#endif

  // Intended for the voice assistant component to access which wake words are available
  // Since these are pointers to the WakeWordModel objects, the voice assistant component can enable or disable them
  std::vector<WakeWordModel *> get_wake_words();

 protected:
  microphone::MicrophoneSource *microphone_source_{nullptr};
  Trigger<std::string> wake_word_detected_trigger_;
  State state_{State::STOPPED};

  std::weak_ptr<ring_buffer::RingBuffer> ring_buffer_;
  std::vector<WakeWordModel *> wake_word_models_;
  std::vector<std::unique_ptr<WakeWordModel>> runtime_models_;

#ifdef USE_MICRO_WAKE_WORD_VAD
  std::unique_ptr<VADModel> vad_model_;
  bool vad_state_{false};
#endif

  bool pending_start_{false};
  bool pending_stop_{false};

  bool stop_after_detection_;

  bool task_stack_in_psram_{false};

  uint8_t features_step_size_;

  // Audio frontend handles generating spectrogram features
  struct FrontendConfig frontend_config_;
  struct FrontendState frontend_state_;

  // Handles managing the stop/state of the inference task
  EventGroupHandle_t event_group_;

  // Used to send messages about the models' states to the main loop
  QueueHandle_t detection_queue_;

  StaticTask inference_task_;

  static void inference_task(void *params);

  /// @brief Suspends the inference task
  void suspend_task_();
  /// @brief Resumes the inference task
  void resume_task_();

  /// @brief Parks the inference task at a safe point (or verifies it isn't running) so the model lists may be
  /// mutated from the main loop. Every successful call must be paired with unlock_models_().
  /// @return True if the lists may be mutated, false if the running task never acknowledged the pause request
  bool try_lock_models_();
  /// @brief Releases the inference task parked by a successful try_lock_models_() call
  void unlock_models_();

  void set_state_(State state);

  /// @brief Generates a spectrogram feature from an input buffer of audio samples. The frontend buffers samples
  /// internally, so callers may stream arbitrary-sized chunks; a feature is only emitted once enough samples have
  /// accumulated to fill a full analysis window.
  /// @param audio_buffer (const int16_t *) Buffer containing input audio samples
  /// @param samples_available (size_t) Number of samples available in the input buffer
  /// @param features_buffer (int8_t *) Buffer to store the generated feature, valid only when the return value is true
  /// @param processed_samples (size_t *) Set to the number of samples consumed from the input buffer
  /// @return True if a new feature was generated; false if more samples are required
  bool generate_features_(const int16_t *audio_buffer, size_t samples_available,
                          int8_t features_buffer[PREPROCESSOR_FEATURE_SIZE], size_t *processed_samples);

  /// @brief Processes any new probabilities for each model. If any wake word is detected, it will send a DetectionEvent
  /// to the detection_queue_.
  void process_probabilities_();

  /// @brief Deletes each model's TFLite interpreters and frees tensor arena memory.
  void unload_models_();

  /// @brief Runs an inference with each model using the new spectrogram features
  /// @param audio_features (int8_t *) Buffer containing new spectrogram features
  /// @return True if successful, false if any errors were encountered
  bool update_model_probabilities_(const int8_t audio_features[PREPROCESSOR_FEATURE_SIZE]);
};

}  // namespace esphome::micro_wake_word

#endif  // USE_ESP32

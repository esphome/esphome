#include "micro_wake_word.h"

#ifdef USE_ESP_IDF

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esphome/components/audio/audio_transfer_buffer.h"

#ifdef USE_OTA
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome {
namespace micro_wake_word {

static const char *const TAG = "micro_wake_word";

static const ssize_t DETECTION_QUEUE_LENGTH = 5;

static const size_t DATA_TIMEOUT_MS = 50;

static const uint32_t RING_BUFFER_DURATION_MS = 120;

static const uint32_t INFERENCE_TASK_STACK_SIZE = 3072;
static const UBaseType_t INFERENCE_TASK_PRIORITY = 3;

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),  // Signals the inference task should stop

  TASK_STARTING = (1 << 3),
  TASK_RUNNING = (1 << 4),
  TASK_STOPPING = (1 << 5),
  TASK_STOPPED = (1 << 6),

  ERROR_MEMORY = (1 << 9),
  ERROR_INFERENCE = (1 << 10),

  WARNING_FULL_RING_BUFFER = (1 << 13),

  ERROR_BITS = ERROR_MEMORY | ERROR_INFERENCE,
  ALL_BITS = 0xfffff,  // 24 total bits available in an event group
};

float MicroWakeWord::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

static const LogString *micro_wake_word_state_to_string(State state) {
  switch (state) {
    case State::STARTING:
      return LOG_STR("STARTING");
    case State::DETECTING_WAKE_WORD:
      return LOG_STR("DETECTING_WAKE_WORD");
    case State::STOPPING:
      return LOG_STR("STOPPING");
    case State::STOPPED:
      return LOG_STR("STOPPED");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void MicroWakeWord::dump_config() {
  ESP_LOGCONFIG(TAG, "microWakeWord:");
  ESP_LOGCONFIG(TAG, "  models:");
  for (auto &model : this->wake_word_models_) {
    model->log_model_config();
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  this->vad_model_->log_model_config();
#endif
}

void MicroWakeWord::setup() {
  ESP_LOGCONFIG(TAG, "Setting up microWakeWord...");

  this->frontend_config_.window.size_ms = FEATURE_DURATION_MS;
  this->frontend_config_.window.step_size_ms = this->features_step_size_;
  this->frontend_config_.filterbank.num_channels = PREPROCESSOR_FEATURE_SIZE;
  this->frontend_config_.filterbank.lower_band_limit = FILTERBANK_LOWER_BAND_LIMIT;
  this->frontend_config_.filterbank.upper_band_limit = FILTERBANK_UPPER_BAND_LIMIT;
  this->frontend_config_.noise_reduction.smoothing_bits = NOISE_REDUCTION_SMOOTHING_BITS;
  this->frontend_config_.noise_reduction.even_smoothing = NOISE_REDUCTION_EVEN_SMOOTHING;
  this->frontend_config_.noise_reduction.odd_smoothing = NOISE_REDUCTION_ODD_SMOOTHING;
  this->frontend_config_.noise_reduction.min_signal_remaining = NOISE_REDUCTION_MIN_SIGNAL_REMAINING;
  this->frontend_config_.pcan_gain_control.enable_pcan = PCAN_GAIN_CONTROL_ENABLE_PCAN;
  this->frontend_config_.pcan_gain_control.strength = PCAN_GAIN_CONTROL_STRENGTH;
  this->frontend_config_.pcan_gain_control.offset = PCAN_GAIN_CONTROL_OFFSET;
  this->frontend_config_.pcan_gain_control.gain_bits = PCAN_GAIN_CONTROL_GAIN_BITS;
  this->frontend_config_.log_scale.enable_log = LOG_SCALE_ENABLE_LOG;
  this->frontend_config_.log_scale.scale_shift = LOG_SCALE_SCALE_SHIFT;

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

  this->detection_queue_ = xQueueCreate(DETECTION_QUEUE_LENGTH, sizeof(DetectionEvent));
  if (this->detection_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create detection event queue");
    this->mark_failed();
    return;
  }

  this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    if (this->state_ == State::STOPPED) {
      return;
    }
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
    if (this->ring_buffer_.use_count() > 1) {
      size_t bytes_free = temp_ring_buffer->free();

      if (bytes_free < data.size()) {
        xEventGroupSetBits(this->event_group_, EventGroupBits::WARNING_FULL_RING_BUFFER);
        temp_ring_buffer->reset();
      }
      temp_ring_buffer->write((void *) data.data(), data.size());
    }
  });

#ifdef USE_OTA
  ota::get_global_ota_callback()->add_on_state_callback(
      [this](ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) {
        if (state == ota::OTA_STARTED) {
          this->suspend_task_();
        } else if (state == ota::OTA_ERROR) {
          this->resume_task_();
        }
      });
#endif
  ESP_LOGCONFIG(TAG, "Micro Wake Word initialized");
}

void MicroWakeWord::inference_task(void *params) {
  MicroWakeWord *this_mww = (MicroWakeWord *) params;

  xEventGroupSetBits(this_mww->event_group_, EventGroupBits::TASK_STARTING);

  {  // Ensures any C++ objects fall out of scope to deallocate before deleting the task

    const size_t new_bytes_to_process =
        this_mww->microphone_source_->get_audio_stream_info().ms_to_bytes(this_mww->features_step_size_);
    std::unique_ptr<audio::AudioSourceTransferBuffer> audio_buffer;
    int8_t features_buffer[PREPROCESSOR_FEATURE_SIZE];

    if (!(xEventGroupGetBits(this_mww->event_group_) & ERROR_BITS)) {
      // Allocate audio transfer buffer
      audio_buffer = audio::AudioSourceTransferBuffer::create(new_bytes_to_process);

      if (audio_buffer == nullptr) {
        xEventGroupSetBits(this_mww->event_group_, EventGroupBits::ERROR_MEMORY);
      }
    }

    if (!(xEventGroupGetBits(this_mww->event_group_) & ERROR_BITS)) {
      // Allocate ring buffer
      std::shared_ptr<RingBuffer> temp_ring_buffer = RingBuffer::create(
          this_mww->microphone_source_->get_audio_stream_info().ms_to_bytes(RING_BUFFER_DURATION_MS));
      if (temp_ring_buffer.use_count() == 0) {
        xEventGroupSetBits(this_mww->event_group_, EventGroupBits::ERROR_MEMORY);
      }
      audio_buffer->set_source(temp_ring_buffer);
      this_mww->ring_buffer_ = temp_ring_buffer;
    }

    if (!(xEventGroupGetBits(this_mww->event_group_) & ERROR_BITS)) {
      this_mww->microphone_source_->start();
      xEventGroupSetBits(this_mww->event_group_, EventGroupBits::TASK_RUNNING);

      while (!(xEventGroupGetBits(this_mww->event_group_) & COMMAND_STOP)) {
        audio_buffer->transfer_data_from_source(pdMS_TO_TICKS(DATA_TIMEOUT_MS));

        if (audio_buffer->available() < new_bytes_to_process) {
          // Insufficient data to generate new spectrogram features, read more next iteration
          continue;
        }

        // Generate new spectrogram features
        uint32_t processed_samples = this_mww->generate_features_(
            (int16_t *) audio_buffer->get_buffer_start(), audio_buffer->available() / sizeof(int16_t), features_buffer);
        audio_buffer->decrease_buffer_length(processed_samples * sizeof(int16_t));

        // Run inference using the new spectorgram features
        if (!this_mww->update_model_probabilities_(features_buffer)) {
          xEventGroupSetBits(this_mww->event_group_, EventGroupBits::ERROR_INFERENCE);
          break;
        }

        // Process each model's probabilities and possibly send a Detection Event to the queue
        this_mww->process_probabilities_();
      }
    }
  }

  xEventGroupSetBits(this_mww->event_group_, EventGroupBits::TASK_STOPPING);

  this_mww->unload_models_();
  this_mww->microphone_source_->stop();
  FrontendFreeStateContents(&this_mww->frontend_state_);

  xEventGroupSetBits(this_mww->event_group_, EventGroupBits::TASK_STOPPED);
  while (true) {
    // Continuously delay until the main loop deletes the task
    delay(10);
  }
}

std::vector<WakeWordModel *> MicroWakeWord::get_wake_words() {
  std::vector<WakeWordModel *> external_wake_word_models;
  for (auto *model : this->wake_word_models_) {
    if (!model->get_internal_only()) {
      external_wake_word_models.push_back(model);
    }
  }
  return external_wake_word_models;
}

void MicroWakeWord::add_wake_word_model(WakeWordModel *model) { this->wake_word_models_.push_back(model); }

#ifdef USE_MICRO_WAKE_WORD_VAD
void MicroWakeWord::add_vad_model(const uint8_t *model_start, uint8_t probability_cutoff, size_t sliding_window_size,
                                  size_t tensor_arena_size) {
  this->vad_model_ = make_unique<VADModel>(model_start, probability_cutoff, sliding_window_size, tensor_arena_size);
}
#endif

void MicroWakeWord::suspend_task_() {
  if (this->inference_task_handle_ != nullptr) {
    vTaskSuspend(this->inference_task_handle_);
  }
}

void MicroWakeWord::resume_task_() {
  if (this->inference_task_handle_ != nullptr) {
    vTaskResume(this->inference_task_handle_);
  }
}

void MicroWakeWord::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & EventGroupBits::ERROR_MEMORY) {
    xEventGroupClearBits(this->event_group_, EventGroupBits::ERROR_MEMORY);
    ESP_LOGE(TAG, "Encountered an error allocating buffers");
  }

  if (event_group_bits & EventGroupBits::ERROR_INFERENCE) {
    xEventGroupClearBits(this->event_group_, EventGroupBits::ERROR_INFERENCE);
    ESP_LOGE(TAG, "Encountered an error while performing an inference");
  }

  if (event_group_bits & EventGroupBits::WARNING_FULL_RING_BUFFER) {
    xEventGroupClearBits(this->event_group_, EventGroupBits::WARNING_FULL_RING_BUFFER);
    ESP_LOGW(TAG, "Not enough free bytes in ring buffer to store incoming audio data. Resetting the ring buffer. Wake "
                  "word detection accuracy will temporarily be reduced.");
  }

  if (event_group_bits & EventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Inference task has started, attempting to allocate memory for buffers");
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STARTING);
  }

  if (event_group_bits & EventGroupBits::TASK_RUNNING) {
    ESP_LOGD(TAG, "Inference task is running");

    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_RUNNING);
    this->set_state_(State::DETECTING_WAKE_WORD);
  }

  if (event_group_bits & EventGroupBits::TASK_STOPPING) {
    ESP_LOGD(TAG, "Inference task is stopping, deallocating buffers");
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STOPPING);
  }

  if ((event_group_bits & EventGroupBits::TASK_STOPPED)) {
    ESP_LOGD(TAG, "Inference task is finished, freeing task resources");
    vTaskDelete(this->inference_task_handle_);
    this->inference_task_handle_ = nullptr;
    xEventGroupClearBits(this->event_group_, ALL_BITS);
    xQueueReset(this->detection_queue_);
    this->set_state_(State::STOPPED);
  }

  if ((this->pending_start_) && (this->state_ == State::STOPPED)) {
    this->set_state_(State::STARTING);
    this->pending_start_ = false;
  }

  if ((this->pending_stop_) && (this->state_ == State::DETECTING_WAKE_WORD)) {
    this->set_state_(State::STOPPING);
    this->pending_stop_ = false;
  }

  switch (this->state_) {
    case State::STARTING:
      if ((this->inference_task_handle_ == nullptr) && !this->status_has_error()) {
        // Setup preprocesor feature generator. If done in the task, it would lock the task to its initial core, as it
        // uses floating point operations.
        if (!FrontendPopulateState(&this->frontend_config_, &this->frontend_state_,
                                   this->microphone_source_->get_audio_stream_info().get_sample_rate())) {
          this->status_momentary_error(
              "Failed to allocate buffers for spectrogram feature processor, attempting again in 1 second", 1000);
          return;
        }

        xTaskCreate(MicroWakeWord::inference_task, "mww", INFERENCE_TASK_STACK_SIZE, (void *) this,
                    INFERENCE_TASK_PRIORITY, &this->inference_task_handle_);

        if (this->inference_task_handle_ == nullptr) {
          FrontendFreeStateContents(&this->frontend_state_);  // Deallocate frontend state
          this->status_momentary_error("Task failed to start, attempting again in 1 second", 1000);
        }
      }
      break;
    case State::DETECTING_WAKE_WORD: {
      DetectionEvent detection_event;
      while (xQueueReceive(this->detection_queue_, &detection_event, 0)) {
        if (detection_event.blocked_by_vad) {
          ESP_LOGD(TAG, "Wake word model predicts '%s', but VAD model doesn't.", detection_event.wake_word->c_str());
        } else {
          constexpr float uint8_to_float_divisor =
              255.0f;  // Converting a quantized uint8 probability to floating point
          ESP_LOGD(TAG, "Detected '%s' with sliding average probability is %.2f and max probability is %.2f",
                   detection_event.wake_word->c_str(), (detection_event.average_probability / uint8_to_float_divisor),
                   (detection_event.max_probability / uint8_to_float_divisor));
          this->wake_word_detected_trigger_->trigger(*detection_event.wake_word);
          if (this->stop_after_detection_) {
            this->stop();
          }
        }
      }
      break;
    }
    case State::STOPPING:
      xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_STOP);
      break;
    case State::STOPPED:
      break;
  }
}

void MicroWakeWord::start() {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "Wake word detection can't start as the component hasn't been setup yet");
    return;
  }

  if (this->is_failed()) {
    ESP_LOGW(TAG, "Wake word component is marked as failed. Please check setup logs");
    return;
  }

  if (this->is_running()) {
    ESP_LOGW(TAG, "Wake word detection is already running");
    return;
  }

  ESP_LOGD(TAG, "Starting wake word detection");

  this->pending_start_ = true;
  this->pending_stop_ = false;
}

void MicroWakeWord::stop() {
  if (this->state_ == STOPPED)
    return;

  ESP_LOGD(TAG, "Stopping wake word detection");

  this->pending_start_ = false;
  this->pending_stop_ = true;
}

void MicroWakeWord::set_state_(State state) {
  if (this->state_ != state) {
    ESP_LOGD(TAG, "State changed from %s to %s", LOG_STR_ARG(micro_wake_word_state_to_string(this->state_)),
             LOG_STR_ARG(micro_wake_word_state_to_string(state)));
    this->state_ = state;
  }
}

size_t MicroWakeWord::generate_features_(int16_t *audio_buffer, size_t samples_available,
                                         int8_t features_buffer[PREPROCESSOR_FEATURE_SIZE]) {
  size_t processed_samples = 0;
  struct FrontendOutput frontend_output =
      FrontendProcessSamples(&this->frontend_state_, audio_buffer, samples_available, &processed_samples);

  for (size_t i = 0; i < frontend_output.size; ++i) {
    // These scaling values are set to match the TFLite audio frontend int8 output.
    // The feature pipeline outputs 16-bit signed integers in roughly a 0 to 670
    // range. In training, these are then arbitrarily divided by 25.6 to get
    // float values in the rough range of 0.0 to 26.0. This scaling is performed
    // for historical reasons, to match up with the output of other feature
    // generators.
    // The process is then further complicated when we quantize the model. This
    // means we have to scale the 0.0 to 26.0 real values to the -128 (INT8_MIN)
    // to 127 (INT8_MAX) signed integer numbers.
    // All this means that to get matching values from our integer feature
    // output into the tensor input, we have to perform:
    // input = (((feature / 25.6) / 26.0) * 256) - 128
    // To simplify this and perform it in 32-bit integer math, we rearrange to:
    // input = (feature * 256) / (25.6 * 26.0) - 128
    constexpr int32_t value_scale = 256;
    constexpr int32_t value_div = 666;  // 666 = 25.6 * 26.0 after rounding
    int32_t value = ((frontend_output.values[i] * value_scale) + (value_div / 2)) / value_div;

    value += INT8_MIN;  // Adds a -128; i.e., subtracts 128
    features_buffer[i] = static_cast<int8_t>(clamp<int32_t>(value, INT8_MIN, INT8_MAX));
  }

  return processed_samples;
}

void MicroWakeWord::process_probabilities_() {
#ifdef USE_MICRO_WAKE_WORD_VAD
  DetectionEvent vad_state = this->vad_model_->determine_detected();

  this->vad_state_ = vad_state.detected;  // atomic write, so thread safe
#endif

  for (auto &model : this->wake_word_models_) {
    if (model->get_unprocessed_probability_status()) {
      // Only detect wake words if there is a new probability since the last check
      DetectionEvent wake_word_state = model->determine_detected();
      if (wake_word_state.detected) {
#ifdef USE_MICRO_WAKE_WORD_VAD
        if (vad_state.detected) {
#endif
          xQueueSend(this->detection_queue_, &wake_word_state, portMAX_DELAY);
          model->reset_probabilities();
#ifdef USE_MICRO_WAKE_WORD_VAD
        } else {
          wake_word_state.blocked_by_vad = true;
          xQueueSend(this->detection_queue_, &wake_word_state, portMAX_DELAY);
        }
#endif
      }
    }
  }
}

void MicroWakeWord::unload_models_() {
  for (auto &model : this->wake_word_models_) {
    model->unload_model();
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  this->vad_model_->unload_model();
#endif
}

bool MicroWakeWord::update_model_probabilities_(const int8_t audio_features[PREPROCESSOR_FEATURE_SIZE]) {
  bool success = true;

  for (auto &model : this->wake_word_models_) {
    // Perform inference
    success = success & model->perform_streaming_inference(audio_features);
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  success = success & this->vad_model_->perform_streaming_inference(audio_features);
#endif

  return success;
}

}  // namespace micro_wake_word
}  // namespace esphome

#endif  // USE_ESP_IDF

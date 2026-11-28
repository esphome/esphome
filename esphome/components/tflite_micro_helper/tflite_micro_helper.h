#pragma once

#include "model_handler.h"
#include "memory_manager.h"
#include "esphome/core/log.h"

#include "esphome/core/defines.h"

#include <atomic>
#include <mutex>

#ifdef USE_TFLITE_MICRO_HELPER

namespace esphome::tflite_micro_helper {

/**
 * @struct ModelSpec
 * @brief Generic model dimensions and input spec for any TFLite consumer.
 *
 * NOTE: This struct uses IMAGE (4D) tensor semantics. For audio (3D) models,
 * get_input_width/height/channels() return 0 and input_type is derived from
 * input_tensor()->type. Audio consumers MUST read input_tensor()->dims
 * directly and manage their own streaming runtime (MRV, sliding window).
 */
struct ModelSpec {
  int input_width{0};
  int input_height{0};
  int input_channels{0};
  bool normalize{false};
  std::string input_order{"RGB"};
  int input_type{0};  // 0=UINT8, 1=FLOAT
};

/**
 * @struct ArenaStats
 * @brief Tensor arena utilization statistics.
 */
struct ArenaStats {
  size_t total_size{0};
  size_t used_bytes{0};
  size_t wasted_bytes{0};
  float efficiency{0.0f};
};

/**
 * @struct LoadStats
 * @brief Model loading stage timings and final result.
 */
struct LoadStats {
  uint32_t load_start_ms{0};
  uint32_t parse_time_ms{0};  // CRC verify + arena probe
  uint32_t arena_alloc_time_ms{0};
  uint32_t total_load_time_ms{0};
  bool success{false};
};

/**
 * @class TFLiteMicroHelper
 * @brief Reusable TFLite Micro component for ESPHome.
 *
 * Wraps model loading, tensor arena management, config building,
 * and inference invocation. Consumer components set config via setters
 * then call load_model() / run_inference().
 */
class TFLiteMicroHelper {
 public:
  TFLiteMicroHelper() = default;
  ~TFLiteMicroHelper() = default;

  // -- Common Model Config Setters -----------------------------------
  void set_model_type(const std::string &t) { this->model_type_ = t; }
  void set_tensor_arena_size(size_t size);
  void set_model(const uint8_t *model_data, size_t model_size);
  void set_output_processing(const std::string &p) { this->output_processing_ = p; }
  void set_scale_factor(float f) { this->scale_factor_ = f; }
  void set_debug(bool d) {
    this->debug_ = d;
    this->model_handler_.set_debug(d);
  }

  // -- Image Model Config Setters ------------------------------------
  void set_input_type(const std::string &t);
  void set_input_channels(int c);
  void set_input_width(int w);
  void set_input_height(int h);
  void set_input_order(const std::string &o);
  void set_normalize(bool n);
  void set_invert(bool i);

  // -- Audio Model Config Setters ------------------------------------
  void set_probability_cutoff(float f);  // 0.0-1.0, quantized internally
  void set_sliding_window_size(size_t n);
  void set_features_step_size(uint8_t ms);
  void set_feature_count(size_t n);

  // -- Model Integrity ------------------------------------------------
  void set_expected_crc32(uint32_t crc) { this->expected_crc32_ = crc; }

  // -- Lifecycle ------------------------------------------------------
  bool load_model();

  /// @brief Unload the model and free the arena.
  /// @param reset_config If true, also clear all model-config fields via
  ///   reset_config() so a subsequent load_model() starts from clean defaults.
  ///   Use this when switching to a *different* model -- the consumer must then
  ///   re-issue set_model() and all relevant setters before load_model().
  ///   Keep false (default) for same-model reload where config is preserved
  ///   (e.g. meter_reader_tflite::reload_resources).
  void unload_model(bool reset_config = false);

  /// @brief Reset all configuration fields to type-safe defaults (P1).
  /// Prevents a stale arena size, input dimensions, preprocessing, or audio
  /// configuration from leaking into the next load_model() when a consumer
  /// switches to a different model. Must be followed by fresh setters +
  /// set_model() before load_model().
  void reset_config();

  /// @brief Returns true only when the model has been fully loaded (READY).
  /// Uses a single atomic state machine: no check-then-act across multiple flags.
  bool is_model_loaded() const { return this->state_.load() == LoadState::READY; }

  // -- Inference ------------------------------------------------------
  TfLiteStatus invoke() { return this->model_handler_.invoke(); }

  TfLiteTensor *input_tensor() { return this->model_handler_.input_tensor(); }
  const TfLiteTensor *input_tensor() const { return this->model_handler_.input_tensor(); }
  TfLiteTensor *output_tensor() { return this->model_handler_.output_tensor(); }

  bool run_inference_on_buffer(const uint8_t *src_data, size_t src_size);
  ProcessedOutput run_inference(const uint8_t *src_data, size_t src_size);

  // -- Accessors -----------------------------------------------------
  ModelSpec get_model_spec() const;

  int get_input_width() const { return this->model_handler_.get_input_width(); }
  int get_input_height() const { return this->model_handler_.get_input_height(); }
  int get_input_channels() const { return this->model_handler_.get_input_channels(); }
  const ModelConfig &get_config() const { return this->model_handler_.get_config(); }

  // -- Memory ---------------------------------------------------------
  size_t get_arena_used_bytes() const { return this->model_handler_.get_arena_used_bytes(); }
  size_t get_tensor_arena_size() const { return this->tensor_arena_size_requested_; }
  size_t get_tensor_arena_size_actual() const { return this->tensor_arena_allocation_.actual_size; }
  size_t get_model_size_bytes() const { return this->model_length_; }

  ArenaStats get_arena_stats() const;
  void update_arena_stats_cache();
  void report_memory_status();

  // -- Load statistics -------------------------------------------------
  const LoadStats &get_last_load_stats() const { return this->last_load_stats_; }

#ifdef DEBUG_TFLITE_MICRO_HELPER
  void debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data) {
    this->model_handler_.debug_test_parameters(zone_data);
  }
#endif

 private:
  // Model config (set from __init__.py)
  std::string model_type_{"default"};
  std::string input_type_{"uint8"};
  int input_channels_{3};
  int input_width_{32};
  int input_height_{20};
  std::string output_processing_{"direct_class"};
  float scale_factor_{1.0f};
  std::string input_order_{"RGB"};
  bool normalize_{false};
  bool invert_{false};
  bool debug_{false};

  // Model data (set via set_model)
  const uint8_t *model_data_{nullptr};
  size_t model_length_{0};
  size_t tensor_arena_size_requested_{100 * 1024};

  // Image model config
  ImageModelConfig image_config_;

  // Audio model config
  AudioModelConfig audio_config_;

  // Components
  ModelHandler model_handler_;
  MemoryManager::AllocationResult tensor_arena_allocation_;

  // Arena stats cache (thread-safe for dual-core)
  mutable std::mutex arena_stats_mutex_;
  ArenaStats cached_arena_stats_{};

  // Load statistics (last load_model() attempt)
  LoadStats last_load_stats_{};

  // State -- single atomic load state machine (E2 / TOCTOU fix)
  enum class LoadState : uint8_t { UNLOADED, LOADING, READY };
  std::atomic<LoadState> state_{LoadState::UNLOADED};

  // Expected CRC32 (0 = skip verification, set from __init__.py)
  uint32_t expected_crc32_{0};

  // Internals
  bool allocate_tensor_arena_();
  ModelConfig build_config_();
  bool validate_input_tensor_(const uint8_t *src_data, size_t src_size) const;
};

}  // namespace esphome::tflite_micro_helper

#endif  // USE_TFLITE_MICRO_HELPER

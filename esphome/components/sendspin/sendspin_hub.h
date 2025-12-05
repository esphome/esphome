#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF)

#include "sendspin_protocol.h"
#include "sendspin_time_filter.h"
#include "sendspin_websocket.h"

#ifdef USE_SENDSPIN_PLAYER
#include "sendspin_audio_chunk.h"
#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_chunk_queue.h"
#include <vector>
#endif

#ifdef USE_SENDSPIN_ARTWORK
#include <vector>
#endif

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <limits>

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

#ifdef USE_SENDSPIN_SENSOR
enum class SendspinSensorTypes {
  KALMAN_ERROR,
  AUDIBLE_SYNCS,
  HARD_SYNC_FRAMES_ADDED,
  HARD_SYNC_FRAMES_REMOVED,
  SINGLE_SYNC_FRAMES_ADDED,
  SINGLE_SYNC_FRAMES_REMOVED,
};

struct SendspinSensorUpdate {
  SendspinSensorTypes type;
  float value;
};
#endif

#ifdef USE_SENDSPIN_ARTWORK
struct ImageSlotPreference {
  uint8_t slot;
  SendspinImageSource source;
  SendspinImageFormat format;
  uint16_t width;
  uint16_t height;
};

struct ImageSlotCallback {
  uint8_t slot;
  CallbackManager<void(const uint8_t *, size_t, SendspinImageFormat)> callbacks;
};
#endif

// These are meant for internal controls to keep child components in sync
enum class SendspinControls {
  START,
  STOP,
  MUTE_UPDATE,
  VOLUME_UPDATE,
};

class SendspinHub : public Component {
  /* Basic component design: This hub component handles creating the websocket server and interacting with a sendspin
   * server once it connects via websockets. It interacts with the Sendspin server directly over websockets.
   *  - sendspin_protocol.h handles the specific Sendspin protocol messages
   *  - sendspin_time_filter.h uses a Kalman filter to accurately convert the client timestamp into a server timestamp
   *    or vice versa
   *  - sendspin_websocket.h creates a websocket server that a Sendspin server connects to and handles the low level
   *    sending and receiving of messages
   *    - the hub's websocket_server_handler is the callback function that processes the messages at a high level
   *  - sendspin_decoder.h handles decoding audio chunks in a memory efficient way (TODO: generalize and move to the
   *    audio component)
   *
   * The hub sends appropriate data to child Sendspin components using callbacks.
   *  - Track metadata is sent to text sensors
   *  - Decoded audio is sent to a media player. The media player handles playing that audio in sync.
   *  - Any Sendspin component can use the hub's ``update_sendspin_sensor`` function to update diagnostic sensors.
   *    - the acutal sensor updates are sent via callbacks to the sensor components.
   *  - Internal controls for keeping the child components synchronized use ``controls_callback_``
   *  - TODO: Reevaluate this callback structure, as each particular callback follows a slightly different pattern. It
   *    would be nice if it were more uniform.
   */
 public:
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }
  void setup() override;
  void loop() override;

  void start();

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }
  void set_buffer_size(size_t buffer_size) { this->buffer_size_ = buffer_size; }

#ifdef USE_SENDSPIN_PLAYER
  // Simple audio chunk callback registration
  void add_audio_chunk_callback(
      std::function<bool(std::shared_ptr<SendspinAudioChunk>, TickType_t, const audio::AudioStreamInfo &)> &&callback) {
    this->audio_chunk_callbacks_.push_back(std::move(callback));
  }

  uint8_t get_volume() { return this->volume_; }
  bool get_muted() { return this->muted_; }
  void update_muted(bool is_muted);
  void update_volume(uint8_t volume);
  void update_state(SendspinPlayerState state);
  void publish_client_state();

  ServerPlayerStreamObject &get_current_stream_params() { return this->current_stream_params_; }
#endif

  void add_controls_callback(std::function<void(const SendspinControls &)> &&callback) {
    this->controls_callbacks_.add(std::move(callback));
  }

  void add_group_update_callback(std::function<void(const GroupUpdateObject &)> &&callback) {
    this->group_update_callbacks_.add(std::move(callback));
  }

  std::string get_group_id() const { return this->group_state_.group_id.value_or(""); }
  std::string get_group_name() const { return this->group_state_.group_name.value_or(""); }

#ifdef USE_SENDSPIN_METADATA
  void add_metadata_callback(std::function<void(const ServerMetadataStateObject &)> &&callback) {
    this->metadata_callbacks_.add(std::move(callback));
  }
#endif

#ifdef USE_SENDSPIN_SENSOR
  void add_sensor_callback(std::function<void(const SendspinSensorUpdate &)> &&callback) {
    this->sensor_callbacks_.add(std::move(callback));
  }

  void update_sendspin_sensor(SendspinSensorUpdate sensor_update) { this->sensor_callbacks_.call(sensor_update); }
#endif

#ifdef USE_SENDSPIN_CONTROLLER
  void send_client_command(SendspinCommandType command, std::optional<uint8_t> volume = std::nullopt,
                           std::optional<bool> mute = std::nullopt);
#endif

  void set_kalman_process_error(double process_error) { this->kalman_process_error_ = process_error; }
  void set_kalman_forget_factor(double forget_factor) { this->kalman_forget_factor_ = forget_factor; }

#ifdef USE_SENDSPIN_ARTWORK
  void add_image_slot_callback(uint8_t slot,
                               std::function<void(const uint8_t *, size_t, SendspinImageFormat)> &&callback) {
    // Linear search for existing slot
    for (auto &entry : this->image_slot_callbacks_) {
      if (entry.slot == slot) {
        entry.callbacks.add(std::move(callback));
        return;
      }
    }
    // Create new entry for this slot
    this->image_slot_callbacks_.push_back({slot, {}});
    this->image_slot_callbacks_.back().callbacks.add(std::move(callback));
  }
  void add_image_preferred_format(const ImageSlotPreference &preference) {
    this->preferred_image_formats_.push_back(preference);
  }
  const std::vector<ImageSlotPreference> &get_image_preferred_formats() const { return this->preferred_image_formats_; }
#endif

 protected:
#ifdef USE_SENDSPIN_PLAYER
  bool send_audio_chunk_(std::shared_ptr<SendspinAudioChunk> audio_chunk, TickType_t ticks_to_wait,
                         const audio::AudioStreamInfo &stream_info);

  // Simplified audio consumer management with pointer-based approach
  std::vector<std::function<bool(std::shared_ptr<SendspinAudioChunk>, TickType_t, const audio::AudioStreamInfo &)>>
      audio_chunk_callbacks_;

  std::unique_ptr<audio::AudioChunkQueue> encoded_chunk_queue_;

  static void decode_task(void *params);
  TaskHandle_t decode_task_handle_{nullptr};
  StaticTask_t decode_task_stack_;
  StackType_t *decode_task_stack_buffer_{nullptr};

  uint8_t volume_;
  bool muted_;
  SendspinPlayerState state_{SendspinPlayerState::SYNCHRONIZED};
  ServerPlayerStreamObject current_stream_params_{};
#endif

  void send_time_message_();

  /// @brief Processes sendspin binary message
  /// If it returns true, the caller needs to deallocate the payload
  bool process_binary_message_(uint8_t *payload, size_t len);

  // Process JSON message
  // Returns true if message was successfully processed, false otherwise
  bool process_json_message_(const std::string &message, int64_t timestamp);

  void deallocate_websocket_payload_();

  static esp_err_t websocket_server_handler(httpd_req_t *req);
  static void websocket_close_callback(void *context);

  int64_t last_sent_time_message_{std::numeric_limits<int64_t>::max()};

  uint8_t *websocket_payload_{nullptr};
  size_t websocket_write_offset_{0};
  size_t websocket_len_{0};

  bool task_stack_in_psram_{false};
  size_t buffer_size_{1000000};

  bool force_publish_state_{false};

  bool pending_time_message_{false};
  bool hello_message_sent_{false};
#ifdef USE_WIFI
  bool high_performance_networking_requested_for_time_{false};
  bool high_performance_networking_requested_for_playback_{false};
#endif

  double kalman_process_error_;
  double kalman_forget_factor_;

  EventGroupHandle_t event_group_{nullptr};

  std::unique_ptr<SendspinWebsocket> sendspin_websocket_;
  std::unique_ptr<SendspinTimeFilter> time_filter_;

  ServerInformationObject server_information_{};
  GroupUpdateObject group_state_{};

  CallbackManager<void(const SendspinControls &)> controls_callbacks_{};
  CallbackManager<void(const GroupUpdateObject &)> group_update_callbacks_{};

#ifdef USE_SENDSPIN_ARTWORK
  std::vector<ImageSlotCallback> image_slot_callbacks_;
  std::vector<ImageSlotPreference> preferred_image_formats_;
#endif

#ifdef USE_SENDSPIN_METADATA
  CallbackManager<void(const ServerMetadataStateObject &)> metadata_callbacks_{};
  ServerMetadataStateObject metadata_;
#endif

#ifdef USE_SENDSPIN_SENSOR
  CallbackManager<void(const SendspinSensorUpdate &)> sensor_callbacks_{};
#endif
};

}  // namespace sendspin
}  // namespace esphome

#endif

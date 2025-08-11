#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF)

#include "resonate_protocol.h"
#include "resonate_time_filter.h"
#include "resonate_websocket.h"

#ifdef USE_RESONATE_AUDIO
#include "resonate_chunk_queue.h"
#include "esphome/components/audio/audio.h"
#include <vector>
#endif

#ifdef USE_MEDIA_PLAYER
#include "esphome/components/media_player/media_player.h"
#endif

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <limits>

#include <esp_timer.h>

namespace esphome {
namespace resonate {

#ifdef USE_RESONATE_SENSOR
enum class ResonateSensorTypes {
  KALMAN_ERROR,
  AUDIBLE_SYNCS,
  HARD_SYNC_FRAMES_ADDED,
  HARD_SYNC_FRAMES_REMOVED,
  SINGLE_SYNC_FRAMES_ADDED,
  SINGLE_SYNC_FRAMES_REMOVED,
};

struct ResonateSensorUpdate {
  ResonateSensorTypes type;
  float value;
};
#endif

// These are meant for internal controls to keep child components in sync
enum class ResonateControls {
  START,
  STOP,
};

class ResonateHub : public Component {
  /* Basic component design: This hub component handles creating the websocket server and interacting with a resonate
   * server once it connects via websockets. It interacts with the resonate server directly over websockets.
   *  - resonate_protocol.h handles the specific resonate protocol messages
   *  - resonate_time_filter.h uses a Kalman filter to accurately convert the client timestamp into a server timestamp
   *    or vice versa
   *  - resonate_websocket.h creates a websocket server that a resonate server connects to and handles the low level
   *    sending and receiving of messages
   *    - the hub's websocket_server_handler is the callback function that processes the messages at a high level
   *  - resonate_chunk_queue.h creates a memory efficient FreeRTOS queue for passing encoded and decoded audio chunks
   *    (TODO: generalize and move to the audio component)
   *  - resonate_decoder.h handles decoding audio chunks in a memory efficient way (TODO: generalize and move to the
   *    audio component)
   *
   * The hub sends appropriate data to child resonate components using callbacks.
   *  - Track metadata is sent to text sensors
   *  - Decoded audio is sent to a media player. The media player handles playing that audio in sync.
   *  - Any resonate component can use the hub's ``update_resonate_sensor`` function to update diagnostic sensors.
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

  int64_t convert_server_to_client_timestamp(int64_t server_timestamp) const {
    return this->time_filter_->compute_client_time(server_timestamp);
  }

#ifdef USE_RESONATE_AUDIO
  void add_audio_chunk_callback(
      std::function<bool(AudioChunk &, TickType_t, const audio::AudioStreamInfo &)> &&callback) {
    this->audio_chunk_callback_functions_.push_back(std::move(callback));
  }
#endif

  void add_controls_callback(std::function<void(const ResonateControls &)> &&callback) {
    this->controls_callbacks_.add(std::move(callback));
  }

#ifdef USE_RESONATE_METADATA
  void add_metadata_callback(std::function<void(const ResonateMetadata &)> &&callback) {
    this->metadata_callbacks_.add(std::move(callback));
  }
#endif

#ifdef USE_RESONATE_SENSOR
  void add_sensor_callback(std::function<void(const ResonateSensorUpdate &)> &&callback) {
    this->sensor_callbacks_.add(std::move(callback));
  }

  void update_resonate_sensor(ResonateSensorUpdate sensor_update) { this->sensor_callbacks_.call(sensor_update); }
#endif

#ifdef USE_MEDIA_PLAYER
  void send_stream_command(const media_player::MediaPlayerCall &call);
#endif

  void set_kalman_process_error(double process_error) { this->kalman_process_error_ = process_error; }
  void set_kalman_forget_factor(double forget_factor) { this->kalman_forget_factor_ = forget_factor; }

 protected:
#ifdef USE_RESONATE_AUDIO
  bool send_audio_chunk_(AudioChunk &audio_chunk, TickType_t ticks_to_wait, const audio::AudioStreamInfo &stream_info);
  int8_t successful_receivers_{-1};

  // Manually manage audio chunk callbacks as each caller needs an individual copy of the data chunk
  std::vector<std::function<bool(AudioChunk &, TickType_t, const audio::AudioStreamInfo &)>>
      audio_chunk_callback_functions_{};
  std::unique_ptr<ResonateChunkQueue> encoded_chunk_queue_;

  static void decode_task(void *params);
  TaskHandle_t decode_task_handle_{nullptr};
  StaticTask_t decode_task_stack_;
  StackType_t *decode_task_stack_buffer_{nullptr};
#endif

  void handle_json_message_(const std::string &message, int64_t timestamp);

  static esp_err_t websocket_server_handler(httpd_req_t *req);

  optional<uint16_t> volume_;

  int64_t last_sent_time_message_{std::numeric_limits<int64_t>::max()};

  uint8_t *websocket_payload_{nullptr};

  bool task_stack_in_psram_{false};

  bool force_publish_state_{false};

  bool pending_time_message_{false};

  double kalman_process_error_;
  double kalman_forget_factor_;

  EventGroupHandle_t event_group_{nullptr};

  std::unique_ptr<ResonateWebsocket> resonate_websocket_;
  std::unique_ptr<ResonateTimeFilter> time_filter_;

  std::string server_id_{};
  std::string server_name_{};

  CallbackManager<void(const ResonateControls &)> controls_callbacks_{};

#ifdef USE_RESONATE_METADATA
  CallbackManager<void(const ResonateMetadata &)> metadata_callbacks_{};
  ResonateMetadata metadata_;
#endif

#ifdef USE_RESONATE_SENSOR
  CallbackManager<void(const ResonateSensorUpdate &)> sensor_callbacks_{};
#endif
};

}  // namespace resonate
}  // namespace esphome

#endif

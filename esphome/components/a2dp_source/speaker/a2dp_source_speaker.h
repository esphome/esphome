#pragma once

#ifdef USE_ESP32

#include <atomic>
#include <cstdint>
#include <memory>

#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"

#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_gap_bt_api.h>

namespace esphome::a2dp_source {

class A2DPSourceSpeaker final : public Component, public speaker::Speaker {
 public:
  void set_address(const char *address);
  void setup() override;
  void loop() override;
  void dump_config() override;

  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  void start() override;
  void prepare_for_media() override;
  void stop() override;
  void finish() override;
  bool has_buffered_data() const override;
  void set_volume(float volume) override;
  void set_mute_state(bool mute_state) override;
  uint32_t callback_count() const { return this->callback_count_.load(); }
  uint32_t underrun_bytes() const { return this->underrun_bytes_.load(); }
  uint32_t underrun_events() const { return this->underrun_events_.load(); }
  uint32_t max_callback_gap_us() const { return this->max_callback_gap_us_.load(); }
  bool connected() const { return this->connected_.load(); }
  bool is_ready_for_media() const override { return this->connected_.load() && this->avrcp_ready_.load(); }
  bool streaming() const { return this->streaming_.load(); }
  bool profile_ready() const { return this->profile_ready_.load(); }

 protected:
  static void a2dp_event(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
  static void avrcp_event(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);
  static void gap_event(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
  static int32_t audio_data(uint8_t *data, int32_t length);
  void process_event_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
  void process_avrcp_event_(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);

  uint8_t address_[6]{};
  std::unique_ptr<ring_buffer::RingBuffer> pcm_buffer_{};
  bool initialized_{false};
  std::atomic<bool> profile_ready_{false};
  std::atomic<bool> connect_pending_{false};
  std::atomic<bool> disconnect_pending_{false};
  std::atomic<bool> media_check_pending_{false};
  std::atomic<bool> media_start_pending_{false};
  std::atomic<bool> finish_pending_{false};
  std::atomic<bool> connected_{false};
  std::atomic<bool> acl_connected_{false};
  std::atomic<bool> streaming_{false};
  std::atomic<uint32_t> streaming_started_ms_{0};
  std::atomic<bool> requested_{false};
  bool prepared_start_{false};
  std::atomic<uint32_t> callback_count_{0};
  std::atomic<uint32_t> underrun_bytes_{0};
  std::atomic<uint32_t> underrun_events_{0};
  std::atomic<uint32_t> pcm_bytes_{0};
  std::atomic<uint32_t> max_callback_gap_us_{0};
  std::atomic<bool> muted_{false};
  std::atomic<bool> avrcp_connected_{false};
  std::atomic<bool> avrcp_ready_{false};
  std::atomic<int> avrcp_response_volume_{-1};
  std::atomic<uint8_t> avrcp_target_volume_{0};
  std::atomic<uint8_t> avrcp_last_requested_{255};
  uint8_t avrcp_transaction_{0};
  uint8_t avrcp_attempts_{0};
  std::atomic<bool> avrcp_command_pending_{false};
  uint32_t avrcp_command_deadline_ms_{0};
  uint32_t avrcp_connect_deadline_ms_{0};
  std::atomic<bool> received_first_data_{false};
  std::atomic<int64_t> previous_callback_us_{0};
  uint32_t next_connect_attempt_ms_{0};
  uint32_t next_disconnect_attempt_ms_{0};
  uint32_t next_media_check_ms_{0};
  uint8_t connect_attempts_{0};
  std::atomic<bool> connection_failed_{false};
  uint32_t scanner_wait_started_ms_{0};
  bool scanner_stop_starting_requested_{false};
  bool scanner_paused_for_connect_{false};
  std::atomic<uint32_t> acl_disconnected_ms_{0};
};

}  // namespace esphome::a2dp_source

#endif  // USE_ESP32

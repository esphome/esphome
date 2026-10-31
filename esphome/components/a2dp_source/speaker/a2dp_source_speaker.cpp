#include "a2dp_source_speaker.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstring>
#include <iterator>

#include "esphome/components/esp32_ble/ble.h"
#ifdef USE_ESP32_BLE_TRACKER
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#endif
#include "esphome/core/log.h"

#include <esp_a2dp_legacy_api.h>
#include <esp_avrc_api.h>
#include <esp_bt_main.h>
#include <esp_timer.h>

namespace esphome::a2dp_source {

ESPHOME_LOG_TAG(TAG, "a2dp_source");
ESPHOME_LOG_TAG(CONNECTION_TAG, "a2dp_source.connection");
ESPHOME_LOG_TAG(AVRCP_TAG, "a2dp_source.avrcp");
ESPHOME_LOG_TAG(COEX_TAG, "a2dp_source.coexistence");
// ESP-IDF's C callbacks do not accept an instance context.
static A2DPSourceSpeaker *active_speaker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr size_t PCM_BUFFER_BYTES = 32768;
static constexpr uint8_t MAX_CONNECT_ATTEMPTS = 3;

void A2DPSourceSpeaker::set_address(const char *address) {
  if (address == nullptr || std::strlen(address) != 17) {
    return;
  }
  auto hex_value = [](char digit) -> int {
    if (digit >= '0' && digit <= '9')
      return digit - '0';
    if (digit >= 'a' && digit <= 'f')
      return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F')
      return digit - 'A' + 10;
    return -1;
  };
  uint8_t parsed[6];
  for (size_t i = 0; i < 6; i++) {
    const int high = hex_value(address[i * 3]);
    const int low = hex_value(address[i * 3 + 1]);
    if (high < 0 || low < 0 || (i < 5 && address[i * 3 + 2] != ':')) {
      return;
    }
    parsed[i] = static_cast<uint8_t>((high << 4) | low);
  }
  std::copy_n(parsed, 6, this->address_);
}

void A2DPSourceSpeaker::setup() { this->audio_stream_info_ = audio::AudioStreamInfo(16, 2, 44100); }

void A2DPSourceSpeaker::loop() {
  if (!esp32_ble::global_ble->is_active() || esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    this->connected_.store(false);
    this->streaming_.store(false);
    this->profile_ready_.store(false);
    this->avrcp_connected_.store(false);
    this->avrcp_ready_.store(false);
    this->connect_pending_ = false;
    this->disconnect_pending_ = false;
    this->media_check_pending_ = false;
    this->media_start_pending_ = false;
    this->initialized_ = false;
    this->next_connect_attempt_ms_ = 0;
    return;
  }

  if (!this->initialized_) {
    active_speaker = this;
    esp_err_t err = esp_bt_gap_register_callback(A2DPSourceSpeaker::gap_event);
    if (err == ESP_OK) {
      uint8_t iocap = ESP_BT_IO_CAP_IO;
      err = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
    }
    if (err == ESP_OK) {
      esp_bt_pin_code_t pin_code{};
      err = esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin_code);
    }
    if (err == ESP_OK) {
      const esp_err_t page_err = esp_bt_gap_set_page_timeout(0x2000);
      if (page_err != ESP_OK) {
        ESP_LOGW(CONNECTION_TAG, "Could not shorten Classic page timeout: %s", esp_err_to_name(page_err));
      }
    }
    if (err == ESP_OK)
      err = esp_avrc_ct_init();
    if (err == ESP_OK)
      err = esp_avrc_ct_register_callback(A2DPSourceSpeaker::avrcp_event);
    if (err == ESP_OK)
      err = esp_a2d_source_init();
    if (err == ESP_OK)
      err = esp_a2d_register_callback(A2DPSourceSpeaker::a2dp_event);
    if (err == ESP_OK)
      err = esp_a2d_source_register_data_callback(A2DPSourceSpeaker::audio_data);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "A2DP init failed: %s", esp_err_to_name(err));
      this->mark_failed();
      return;
    }
    this->initialized_ = true;
    ESP_LOGD(TAG, "A2DP source initialized");
  }

  if (!this->profile_ready_.load()) {
    esp_a2d_profile_status_t status{};
    if (esp_a2d_get_profile_status(&status) == ESP_OK && status.a2d_src_inited)
      this->profile_ready_.store(true);
  }
  if (!this->profile_ready_.load())
    return;

  if (this->requested_.load())
    this->state_ = this->prepared_start_ ? speaker::STATE_STARTING : speaker::STATE_RUNNING;

  const uint32_t now = millis();
#ifdef USE_ESP32_BLE_TRACKER
  if (this->scanner_paused_for_connect_ &&
      ((this->avrcp_ready_.load() && this->streaming_.load() &&
        static_cast<int32_t>(now - this->streaming_started_ms_.load()) >= 3000) ||
       (!this->requested_.load() && !this->connected_.load() && !this->acl_connected_.load() &&
        !this->connect_pending_ && !this->disconnect_pending_ &&
        static_cast<int32_t>(now - this->acl_disconnected_ms_.load()) >= 5000))) {
    auto *tracker = esp32_ble_tracker::global_esp32_ble_tracker;
    if (tracker != nullptr && tracker->get_scanner_state() == esp32_ble_tracker::ScannerState::IDLE) {
      tracker->start_scan();
      this->scanner_paused_for_connect_ = false;
      ESP_LOGD(COEX_TAG, "BLE scan resumed after A2DP transition");
    }
  }
#endif
  if (this->requested_.load() && this->connected_.load()) {
    if (this->avrcp_connect_deadline_ms_ == 0)
      this->avrcp_connect_deadline_ms_ = now + 15000;
    int response = this->avrcp_response_volume_.exchange(-1);
    if (response >= 0 && this->avrcp_command_pending_) {
      this->avrcp_ready_.store(true);
      this->avrcp_command_pending_ = false;
      this->avrcp_attempts_ = 0;
      ESP_LOGD(AVRCP_TAG, "AVRCP receiver accepted volume %d/127", response);
    }
    if (this->avrcp_command_pending_ && static_cast<int32_t>(now - this->avrcp_command_deadline_ms_) >= 0)
      this->avrcp_command_pending_ = false;
    if (this->avrcp_connected_.load() && !this->avrcp_command_pending_ &&
        (!this->avrcp_ready_.load() || this->avrcp_last_requested_ != this->avrcp_target_volume_.load())) {
      if (this->avrcp_attempts_ >= 3) {
        ESP_LOGW(AVRCP_TAG, "AVRCP volume was not acknowledged");
        this->stop();
      } else {
        uint8_t target = this->avrcp_target_volume_.load();
        if (esp_avrc_ct_send_set_absolute_volume_cmd(this->avrcp_transaction_++ & 0x0F, target) == ESP_OK) {
          this->avrcp_last_requested_ = target;
          this->avrcp_command_pending_ = true;
          this->avrcp_command_deadline_ms_ = now + 1500;
          this->avrcp_attempts_++;
        } else {
          ESP_LOGW(AVRCP_TAG, "AVRCP volume command failed");
          this->stop();
        }
      }
    }
    if (!this->avrcp_ready_.load() && static_cast<int32_t>(now - this->avrcp_connect_deadline_ms_) >= 0) {
      ESP_LOGW(AVRCP_TAG, "AVRCP preparation timed out");
      this->stop();
    }
  }
  if (this->connection_failed_.exchange(false)) {
    if (this->requested_.load() && this->connect_attempts_ >= MAX_CONNECT_ATTEMPTS) {
      ESP_LOGW(CONNECTION_TAG, "A2DP connection failed after %u attempts", this->connect_attempts_);
      this->stop();
    } else if (this->requested_.load()) {
      this->next_connect_attempt_ms_ = now + 3000 + (now % 2000);
    }
  }
  if (this->connect_pending_ && static_cast<int32_t>(now - this->next_connect_attempt_ms_) >= 0) {
    ESP_LOGW(CONNECTION_TAG, "A2DP connection timed out");
    this->connect_pending_ = false;
    esp_a2d_source_disconnect(this->address_);
    this->connection_failed_ = true;
  }
  if (this->media_check_pending_ && static_cast<int32_t>(now - this->next_media_check_ms_) >= 0)
    this->media_check_pending_ = false;
  if (this->media_start_pending_ && static_cast<int32_t>(now - this->next_media_check_ms_) >= 0)
    this->media_start_pending_ = false;
  if (this->requested_.load()) {
    const uint32_t last_acl_disconnect = this->acl_disconnected_ms_.load();
    if (!this->connected_.load() && !this->acl_connected_.load() && !this->connect_pending_ &&
        !this->disconnect_pending_ && static_cast<int32_t>(now - this->next_connect_attempt_ms_) >= 0 &&
        (last_acl_disconnect == 0 || static_cast<int32_t>(now - last_acl_disconnect) >= 5000)) {
#ifdef USE_ESP32_BLE_TRACKER
      auto *tracker = esp32_ble_tracker::global_esp32_ble_tracker;
      if (tracker != nullptr) {
        const auto scanner_state = tracker->get_scanner_state();
        if (scanner_state != esp32_ble_tracker::ScannerState::STARTING)
          this->scanner_stop_starting_requested_ = false;
        if (scanner_state == esp32_ble_tracker::ScannerState::IDLE ||
            scanner_state == esp32_ble_tracker::ScannerState::RUNNING ||
            scanner_state == esp32_ble_tracker::ScannerState::FAILED) {
          if (!this->scanner_paused_for_connect_) {
            ESP_LOGD(COEX_TAG, "Pausing BLE scan for A2DP connection");
          }
          tracker->stop_scan();
          this->scanner_paused_for_connect_ = true;
        } else if (scanner_state == esp32_ble_tracker::ScannerState::STARTING &&
                   !this->scanner_stop_starting_requested_) {
          tracker->stop_scan();
          this->scanner_stop_starting_requested_ = true;
          this->scanner_paused_for_connect_ = true;
        }
        if (scanner_state != esp32_ble_tracker::ScannerState::IDLE) {
          if (this->scanner_wait_started_ms_ == 0)
            this->scanner_wait_started_ms_ = now;
          if (static_cast<int32_t>(now - this->scanner_wait_started_ms_) >= 5000) {
            ESP_LOGW(COEX_TAG, "BLE scan did not stop before A2DP connect");
            this->stop();
          }
          return;
        }
        this->scanner_wait_started_ms_ = 0;
      }
#endif
      this->connect_attempts_++;
      esp_err_t err = esp_a2d_source_connect(this->address_);
      this->next_connect_attempt_ms_ = now + 15000;
      if (err == ESP_OK) {
        this->connect_pending_ = true;
      } else {
        ESP_LOGW(CONNECTION_TAG, "A2DP connect failed: %s", esp_err_to_name(err));
        this->connection_failed_ = true;
      }
    } else if (this->connected_.load() && this->avrcp_ready_.load() && this->received_first_data_.load() &&
               !this->streaming_.load() && !this->media_check_pending_ && !this->media_start_pending_ &&
               static_cast<int32_t>(now - this->next_media_check_ms_) >= 0) {
      if (esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) == ESP_OK)
        this->media_check_pending_ = true;
      this->next_media_check_ms_ = now + 1000;
    }
  } else if ((this->connected_.load() || this->connect_pending_) && !this->disconnect_pending_ &&
             static_cast<int32_t>(now - this->next_disconnect_attempt_ms_) >= 0) {
    this->next_disconnect_attempt_ms_ = now + 1000;
    if (this->streaming_.load())
      esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
    const esp_err_t err = esp_a2d_source_disconnect(this->address_);
    if (err == ESP_OK) {
      this->disconnect_pending_ = true;
    } else {
      ESP_LOGW(CONNECTION_TAG, "A2DP disconnect request failed: %s", esp_err_to_name(err));
    }
  }

  if (this->finish_pending_ && !this->has_buffered_data())
    this->stop();
}

void A2DPSourceSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG, "A2DP Source Speaker:");
  ESP_LOGCONFIG(TAG, "  Connected: %s", YESNO(this->connected_.load()));
  ESP_LOGCONFIG(TAG, "  Streaming: %s", YESNO(this->streaming_.load()));
  ESP_LOGCONFIG(TAG, "  PCM bytes: %" PRIu32, this->pcm_bytes_.load());
  ESP_LOGCONFIG(TAG, "  Underrun bytes: %" PRIu32, this->underrun_bytes_.load());
}

void A2DPSourceSpeaker::set_volume(float volume) {
  this->volume_ = std::clamp(volume, 0.0f, 1.0f);
  this->avrcp_target_volume_.store(static_cast<uint8_t>(std::lround(this->volume_ * 127.0f)));
}

void A2DPSourceSpeaker::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  this->muted_.store(mute_state);
}

void A2DPSourceSpeaker::start() {
  if (this->prepared_start_ && this->requested_.load()) {
    this->prepared_start_ = false;
    this->state_ = speaker::STATE_RUNNING;
    return;
  }
  if (std::all_of(std::begin(this->address_), std::end(this->address_), [](uint8_t octet) { return octet == 0; })) {
    ESP_LOGW(CONNECTION_TAG, "Speaker address is not configured");
    this->state_ = speaker::STATE_STOPPED;
    return;
  }
  if (!this->pcm_buffer_) {
    this->pcm_buffer_ = ring_buffer::RingBuffer::create(PCM_BUFFER_BYTES);
    if (!this->pcm_buffer_) {
      ESP_LOGE(TAG, "Unable to allocate PCM buffer");
      return;
    }
  }
  this->pcm_buffer_->reset();
  this->connect_attempts_ = 0;
  this->connection_failed_ = false;
  this->avrcp_ready_.store(false);
  this->avrcp_last_requested_ = 255;
  this->avrcp_connect_deadline_ms_ = 0;
  this->avrcp_attempts_ = 0;
  this->avrcp_command_pending_ = false;
  this->avrcp_response_volume_.store(-1);
  this->scanner_wait_started_ms_ = 0;
  this->next_connect_attempt_ms_ = 0;
  this->next_disconnect_attempt_ms_ = 0;
  this->finish_pending_ = false;
  this->received_first_data_.store(false);
  this->streaming_started_ms_.store(0);
  this->previous_callback_us_ = 0;
  this->requested_.store(true);
  this->state_ = speaker::STATE_STARTING;
}

void A2DPSourceSpeaker::prepare_for_media() {
  if (this->requested_.load())
    return;
  this->prepared_start_ = true;
  this->start();
}

void A2DPSourceSpeaker::stop() {
  this->requested_.store(false);
  this->prepared_start_ = false;
  this->avrcp_ready_.store(false);
  this->finish_pending_ = false;
  this->state_ = speaker::STATE_STOPPED;
}

void A2DPSourceSpeaker::finish() { this->finish_pending_ = true; }

bool A2DPSourceSpeaker::has_buffered_data() const { return this->pcm_buffer_ && this->pcm_buffer_->available() != 0; }

size_t A2DPSourceSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (!this->requested_.load() || !this->pcm_buffer_)
    return 0;
  size_t written = this->pcm_buffer_->write_without_replacement(data, length, ticks_to_wait);
  if (written)
    this->received_first_data_.store(true);
  return written;
}

void A2DPSourceSpeaker::a2dp_event(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  if (active_speaker)
    active_speaker->process_event_(event, param);
}

void A2DPSourceSpeaker::avrcp_event(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (active_speaker)
    active_speaker->process_avrcp_event_(event, param);
}

void A2DPSourceSpeaker::process_avrcp_event_(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT && std::memcmp(param->conn_stat.remote_bda, this->address_, 6) == 0) {
    this->avrcp_connected_.store(param->conn_stat.connected);
    ESP_LOGD(AVRCP_TAG, "AVRCP %s",
             param->conn_stat.connected ? LOG_STR_LITERAL("connected") : LOG_STR_LITERAL("disconnected"));
    this->avrcp_ready_.store(false);
    this->avrcp_command_pending_ = false;
    this->avrcp_last_requested_ = 255;
  } else if (event == ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT && this->avrcp_connected_.load()) {
    this->avrcp_response_volume_.store(param->set_volume_rsp.volume);
  }
}

void A2DPSourceSpeaker::gap_event(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (!active_speaker)
    return;
  auto *speaker = active_speaker;
  if (event == ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT &&
      std::memcmp(param->acl_conn_cmpl_stat.bda, speaker->address_, 6) == 0) {
    bool connected = param->acl_conn_cmpl_stat.stat == ESP_BT_STATUS_SUCCESS;
    speaker->acl_connected_.store(connected);
    ESP_LOGD(CONNECTION_TAG, "Classic ACL connect status 0x%02x", param->acl_conn_cmpl_stat.stat);
  } else if (event == ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT &&
             std::memcmp(param->acl_disconn_cmpl_stat.bda, speaker->address_, 6) == 0) {
    speaker->acl_connected_.store(false);
    speaker->acl_disconnected_ms_.store(millis());
    ESP_LOGD(CONNECTION_TAG, "Classic ACL disconnected (reason 0x%02x)", param->acl_disconn_cmpl_stat.reason);
  } else if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
      ESP_LOGD(CONNECTION_TAG, "Bluetooth Classic speaker authenticated");
    } else {
      ESP_LOGW(CONNECTION_TAG, "Bluetooth Classic authentication failed: %d", param->auth_cmpl.stat);
    }
  } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
    bool expected = std::memcmp(param->cfm_req.bda, speaker->address_, 6) == 0;
    esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, expected);
  } else if (event == ESP_BT_GAP_PIN_REQ_EVT) {
    esp_bt_pin_code_t pin{};
    esp_bt_gap_pin_reply(param->pin_req.bda, false, 0, pin);
  }
}

void A2DPSourceSpeaker::process_event_(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  if (event == ESP_A2D_PROF_STATE_EVT) {
    this->profile_ready_.store(param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS);
  } else if (event == ESP_A2D_CONNECTION_STATE_EVT) {
    if (std::memcmp(param->conn_stat.remote_bda, this->address_, 6) != 0)
      return;
    ESP_LOGD(CONNECTION_TAG, "A2DP connection state %d", param->conn_stat.state);
    if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
      if (!this->connected_.exchange(true)) {
        ESP_LOGI(CONNECTION_TAG, "A2DP connected to %02X:%02X:%02X:%02X:%02X:%02X", this->address_[0],
                 this->address_[1], this->address_[2], this->address_[3], this->address_[4], this->address_[5]);
      }
      this->connect_pending_ = false;
      this->disconnect_pending_ = false;
      this->connection_failed_ = false;
    } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
      if (this->connected_.exchange(false)) {
        ESP_LOGI(CONNECTION_TAG, "A2DP disconnected from %02X:%02X:%02X:%02X:%02X:%02X", this->address_[0],
                 this->address_[1], this->address_[2], this->address_[3], this->address_[4], this->address_[5]);
      }
      this->connect_pending_ = false;
      this->disconnect_pending_ = false;
      this->avrcp_connected_.store(false);
      this->avrcp_ready_.store(false);
      this->streaming_.store(false);
      this->media_check_pending_ = false;
      this->media_start_pending_ = false;
      if (this->requested_.load())
        this->connection_failed_ = true;
    }
  } else if (event == ESP_A2D_AUDIO_STATE_EVT) {
    bool streaming = param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED;
    if (streaming)
      this->streaming_started_ms_.store(millis());
    this->streaming_.store(streaming);
  } else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT) {
    if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
      this->media_check_pending_ = false;
      if (param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS && this->requested_.load()) {
        if (esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START) == ESP_OK)
          this->media_start_pending_ = true;
      }
    } else if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START) {
      this->media_start_pending_ = false;
    }
  }
}

int32_t A2DPSourceSpeaker::audio_data(uint8_t *data, int32_t length) {
  if (!active_speaker || !data || length <= 0)
    return 0;
  auto *speaker = active_speaker;
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  speaker->callback_count_.fetch_add(1);
#endif
  const int64_t now_us = esp_timer_get_time();
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  if (speaker->previous_callback_us_ > 0) {
    const int64_t gap = now_us - speaker->previous_callback_us_;
    if (gap > 0 && gap < UINT32_MAX) {
      uint32_t previous_max = speaker->max_callback_gap_us_.load();
      while (static_cast<uint32_t>(gap) > previous_max &&
             !speaker->max_callback_gap_us_.compare_exchange_weak(previous_max, static_cast<uint32_t>(gap))) {
      }
    }
  }
  speaker->previous_callback_us_ = now_us;
#endif
  size_t bytes = speaker->requested_.load() && speaker->pcm_buffer_ ? speaker->pcm_buffer_->read(data, length) : 0;
  if (bytes < static_cast<size_t>(length)) {
    std::memset(data + bytes, 0, length - bytes);
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
    if (speaker->requested_.load() && !speaker->finish_pending_.load() && speaker->received_first_data_.load()) {
      speaker->underrun_bytes_.fetch_add(length - bytes);
      speaker->underrun_events_.fetch_add(1);
    }
#endif
  }
#if defined(USE_LOGGER) && defined(ESPHOME_LOG_HAS_DEBUG)
  speaker->pcm_bytes_.fetch_add(bytes);
#endif
  const bool muted = !speaker->avrcp_ready_.load() || speaker->avrcp_command_pending_.load() ||
                     speaker->avrcp_target_volume_.load() != speaker->avrcp_last_requested_.load() ||
                     speaker->muted_.load() || speaker->avrcp_target_volume_.load() == 0;
  if (muted) {
    std::memset(data, 0, length);
  } else if (bytes != 0) {
    speaker->audio_output_callback_(bytes / 4, now_us);
  }
  return length;
}

}  // namespace esphome::a2dp_source

#endif  // USE_ESP32

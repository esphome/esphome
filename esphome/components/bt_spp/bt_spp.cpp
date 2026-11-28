#include "bt_spp.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_bt.h>
#include <esp_bt_main.h>

#include <algorithm>
#include <cinttypes>
#include <cstring>

#ifdef USE_ARDUINO
// Keep Arduino from releasing the Bluetooth controller memory at startup (esp32-hal-misc.c).
extern "C" bool btInUse() { return true; }  // NOLINT(readability-identifier-naming)
#endif

namespace esphome::bt_spp {

ESPHOME_LOG_TAG(TAG, "bt_spp");

// Covers an SDP query plus a page timeout, so it only fires when Bluedroid never answers.
static constexpr uint32_t CONNECT_TIMEOUT_MS = 30000;
static constexpr uint32_t DROP_LOG_INTERVAL_MS = 5000;
// Largest block handed to esp_spp_write() at once; RFCOMM splits it into frames for the peer.
static constexpr size_t MAX_WRITE_SIZE = ESP_SPP_MAX_MTU;

static constexpr uint32_t RETRY_ID = 0;
static constexpr uint32_t CONNECT_TIMEOUT_ID = 1;

BtSppComponent *BtSppComponent::instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

BtSppComponent::BtSppComponent(uint64_t address, uint8_t *buffer, size_t rx_size, size_t tx_size)
    : rx_buffer_(buffer), tx_buffer_(buffer + rx_size + 1), rx_size_(rx_size), tx_size_(tx_size) {
  for (size_t i = 0; i < ESP_BD_ADDR_LEN; i++)
    this->address_[i] = static_cast<uint8_t>(address >> ((ESP_BD_ADDR_LEN - 1 - i) * 8));
  this->rx_buffer_size_ = rx_size;
}

static bool check_ok(esp_err_t err, const char *what) {
  if (err == ESP_OK)
    return true;
  ESP_LOGE(TAG, "%s failed: %s", what, esp_err_to_name(err));
  return false;
}

void BtSppComponent::setup() {
  instance = this;
  this->events_ = xQueueCreateStatic(EVENT_QUEUE_LENGTH, sizeof(Event), this->events_buffer_, &this->events_storage_);
  this->rx_stream_ = xStreamBufferCreateStatic(this->rx_size_ + 1, 1, this->rx_buffer_, &this->rx_stream_storage_);

  // BLE is never used here; give its controller memory back to the heap.
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    if (!check_ok(esp_bt_controller_init(&cfg), LOG_STR_LITERAL("esp_bt_controller_init"))) {
      this->mark_failed(LOG_STR("Bluetooth controller init failed"));
      return;
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED &&
      !check_ok(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT), LOG_STR_LITERAL("esp_bt_controller_enable"))) {
    this->mark_failed(LOG_STR("Bluetooth controller enable failed"));
    return;
  }
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED &&
      !check_ok(esp_bluedroid_init(), LOG_STR_LITERAL("esp_bluedroid_init"))) {
    this->mark_failed(LOG_STR("Bluedroid init failed"));
    return;
  }
  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED &&
      !check_ok(esp_bluedroid_enable(), LOG_STR_LITERAL("esp_bluedroid_enable"))) {
    this->mark_failed(LOG_STR("Bluedroid enable failed"));
    return;
  }
  if (!check_ok(esp_bt_gap_register_callback(gap_callback), LOG_STR_LITERAL("esp_bt_gap_register_callback")) ||
      !check_ok(esp_spp_register_callback(spp_callback), LOG_STR_LITERAL("esp_spp_register_callback"))) {
    this->mark_failed(LOG_STR("Bluetooth callback registration failed"));
    return;
  }

  esp_spp_cfg_t spp_cfg{};
  spp_cfg.mode = ESP_SPP_MODE_CB;
  spp_cfg.enable_l2cap_ertm = true;
  spp_cfg.tx_buffer_size = 0;  // only used in VFS mode
  if (!check_ok(esp_spp_enhanced_init(&spp_cfg), LOG_STR_LITERAL("esp_spp_enhanced_init"))) {
    this->mark_failed(LOG_STR("SPP init failed"));
    return;
  }
  // The rest of the setup runs when ESP_SPP_INIT_EVT arrives.
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->connected_binary_sensor_, false);
#endif
}

void BtSppComponent::dump_config() {
  char address[18];
  ESP_LOGCONFIG(TAG,
                "Bluetooth SPP:\n"
                "  Address: %s\n"
                "  Reconnect Interval: %" PRIu32 "ms\n"
                "  Enabled: %s\n"
                "  PIN Code: %s\n"
                "  RX Buffer Size: %u\n"
                "  TX Buffer Size: %u",
                format_mac_addr_upper(this->address_, address), this->reconnect_interval_, YESNO(this->enabled_),
                this->pin_code_.empty() ? LOG_STR_LITERAL("not set") : LOG_STR_LITERAL("set"),
                static_cast<unsigned>(this->rx_size_), static_cast<unsigned>(this->tx_size_));
  if (this->channel_ != 0) {
    ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  } else {
    ESP_LOGCONFIG(TAG, "  Channel: found by SDP query");
  }
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_binary_sensor_);
#endif
}

void BtSppComponent::on_shutdown() {
  // Best effort: the disconnect is queued to Bluedroid and may not go out before the reboot. When it does, the
  // remote device does not wait for its own link timeout.
  if (this->handle_ != 0)
    esp_spp_disconnect(this->handle_);
}

void BtSppComponent::connect() {
  this->enabled_ = true;
  if (this->state_ == State::STATE_IDLE)
    this->start_attempt_();
}

void BtSppComponent::disconnect() {
  this->enabled_ = false;
  this->cancel_timeout(RETRY_ID);
  if (this->handle_ != 0) {
    esp_spp_disconnect(this->handle_);
  } else if (this->pending_handle_ != 0) {
    esp_spp_disconnect(this->pending_handle_);
  }
}

// ---------------- Main loop ----------------

void BtSppComponent::loop() {
  Event event;
  while (xQueueReceive(this->events_, &event, 0) == pdTRUE)
    this->handle_event_(event);

  if (this->state_ == State::STATE_CONNECTED)
    this->send_pending_();

  const uint32_t rx_dropped = this->rx_dropped_.exchange(0, std::memory_order_relaxed);
  const uint32_t events_dropped = this->events_dropped_.exchange(0, std::memory_order_relaxed);
  if (rx_dropped != 0) {
    ESP_LOGW(TAG, "RX buffer full, dropped %" PRIu32 " bytes", rx_dropped);
  }
  if (events_dropped != 0) {
    // A lost open, close or write event would leave the state wrong, so start over.
    ESP_LOGE(TAG, "Event queue full, dropped %" PRIu32 " events; resetting the connection", events_dropped);
    this->reset_link_();
  }
}

void BtSppComponent::start_attempt_() {
  this->cancel_timeout(RETRY_ID);
  // Unread bytes of the last session stay readable while down, never into the next one.
  xStreamBufferReset(this->rx_stream_);
  this->has_peek_ = false;
  this->tx_len_ = 0;
  // Only fires when Bluedroid never answers; a late open event for this attempt is closed in handle_event_().
  this->set_timeout(CONNECT_TIMEOUT_ID, CONNECT_TIMEOUT_MS, [this]() {
    if (this->enabled_) {
      ESP_LOGW(TAG, "No answer from Bluetooth stack, connection attempt cancelled");
    }
    if (this->pending_handle_ != 0)
      esp_spp_disconnect(this->pending_handle_);
    this->pending_handle_ = 0;
    this->attempt_failed_();
  });

  uint8_t channel = this->channel_ != 0 ? this->channel_ : this->found_channel_;
  if (channel != 0) {
    this->connect_channel_(channel);
    return;
  }
  ESP_LOGD(TAG, "Looking up the SPP channel");
  esp_err_t err = esp_spp_start_discovery(this->address_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_spp_start_discovery failed: %s", esp_err_to_name(err));
    this->attempt_failed_();
    return;
  }
  this->set_state_(State::STATE_DISCOVERING);
}

void BtSppComponent::connect_channel_(uint8_t channel) {
  ESP_LOGD(TAG, "Connecting to channel %u", channel);
  // Authentication and encryption are left to the remote device, which asks for pairing if it wants it.
  esp_err_t err = esp_spp_connect(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER, channel, this->address_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_spp_connect failed: %s", esp_err_to_name(err));
    this->attempt_failed_();
    return;
  }
  this->set_state_(State::STATE_CONNECTING);
}

void BtSppComponent::attempt_failed_() {
  this->cancel_timeout(CONNECT_TIMEOUT_ID);
  this->set_state_(State::STATE_IDLE);
  if (!this->enabled_)
    return;  // cancelled by disconnect()
  // The device may have moved its service; look the channel up again next time.
  this->found_channel_ = 0;
  if (!this->failure_logged_) {
    ESP_LOGW(TAG, "Connection failed, retrying every %" PRIu32 "s", this->reconnect_interval_ / 1000);
    this->failure_logged_ = true;
  } else {
    ESP_LOGD(TAG, "Connection failed");
  }
  this->schedule_retry_();
}

void BtSppComponent::schedule_retry_() {
  if (!this->enabled_)
    return;
  this->set_timeout(RETRY_ID, this->reconnect_interval_, [this]() {
    if (this->state_ == State::STATE_IDLE && this->enabled_)
      this->start_attempt_();
  });
}

void BtSppComponent::reset_link_() {
  this->cancel_timeout(CONNECT_TIMEOUT_ID);
  if (this->handle_ != 0)
    esp_spp_disconnect(this->handle_);
  if (this->pending_handle_ != 0)
    esp_spp_disconnect(this->pending_handle_);
  this->pending_handle_ = 0;
  this->rx_handle_.store(0, std::memory_order_relaxed);
  if (this->state_ == State::STATE_STARTING || this->state_ == State::STATE_IDLE)
    return;
  this->set_state_(State::STATE_IDLE);
  this->schedule_retry_();
}

void BtSppComponent::accept_connection_(uint32_t handle) {
  this->cancel_timeout(CONNECT_TIMEOUT_ID);
  this->pending_handle_ = 0;
  this->handle_ = handle;
  this->failure_logged_ = false;
  // Normally the Bluedroid task has already routed this connection's bytes to the RX buffer.
  if (this->rx_handle_.exchange(handle, std::memory_order_relaxed) != handle) {
    xStreamBufferReset(this->rx_stream_);
    this->has_peek_ = false;
  }
  this->set_state_(State::STATE_CONNECTED);
  ESP_LOGI(TAG, "Connected");
}

void BtSppComponent::reject_connection_(uint32_t handle) {
  // Opened after its attempt timed out, or after disconnect().
  esp_spp_disconnect(handle);
  uint32_t expected = handle;
  if (this->rx_handle_.compare_exchange_strong(expected, 0, std::memory_order_relaxed)) {
    xStreamBufferReset(this->rx_stream_);
    this->has_peek_ = false;
  }
}

void BtSppComponent::set_state_(State state) {
  const bool was_connected = this->is_connected();
  this->state_ = state;
  const bool connected = this->is_connected();
  if (connected == was_connected)
    return;
  if (!connected) {
    this->handle_ = 0;
    this->tx_len_ = 0;
    this->write_pending_ = false;
    this->congested_ = false;
  }
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->connected_binary_sensor_, connected);
#endif
}

void BtSppComponent::handle_event_(const Event &event) {
  switch (event.type) {
    case EventType::EVENT_TYPE_INIT: {
      if (event.status != ESP_SPP_SUCCESS) {
        ESP_LOGE(TAG, "SPP init failed, status %u", event.status);
        this->status_set_error(LOG_STR("SPP init failed"));
        return;
      }
      const char *name = this->name_.empty() ? App.get_name().c_str() : this->name_.c_str();
      esp_bt_gap_set_device_name(name);
      // A client only: not discoverable, and nobody may connect to it.
      esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
      // Secure Simple Pairing without a display or keyboard ("Just Works").
      esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
      esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
      // Legacy pairing: the PIN is given when the remote device asks for it.
      esp_bt_pin_code_t pin_code{};
      esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin_code);
      this->set_state_(State::STATE_IDLE);
      if (this->enabled_)
        this->start_attempt_();
      break;
    }
    case EventType::EVENT_TYPE_DISCOVERY:
      if (this->state_ != State::STATE_DISCOVERING)
        break;  // a query that timed out
      if (!this->enabled_) {
        this->attempt_failed_();  // cancelled by disconnect(), not logged
        break;
      }
      if (event.status != ESP_SPP_SUCCESS || event.value == 0) {
        ESP_LOGD(TAG, "No SPP service found, status %u", event.status);
        this->attempt_failed_();
        break;
      }
      this->found_channel_ = static_cast<uint8_t>(event.value);
      this->connect_channel_(this->found_channel_);
      break;
    case EventType::EVENT_TYPE_CLIENT_INIT:
      if (this->state_ != State::STATE_CONNECTING || this->pending_handle_ != 0) {
        // Belongs to an attempt that was already given up.
        if (event.status == ESP_SPP_SUCCESS)
          esp_spp_disconnect(event.handle);
        break;
      }
      if (event.status != ESP_SPP_SUCCESS) {
        ESP_LOGD(TAG, "SPP client init failed, status %u", event.status);
        this->attempt_failed_();
        break;
      }
      this->pending_handle_ = event.handle;
      break;
    case EventType::EVENT_TYPE_OPEN: {
      const bool current = this->state_ == State::STATE_CONNECTING && event.handle == this->pending_handle_;
      if (event.status != ESP_SPP_SUCCESS) {
        if (current) {
          this->pending_handle_ = 0;
          this->attempt_failed_();
        }
        break;
      }
      if (!current || !this->enabled_) {
        this->reject_connection_(event.handle);
        break;
      }
      this->accept_connection_(event.handle);
      break;
    }
    case EventType::EVENT_TYPE_CLOSE:
      if (this->state_ == State::STATE_CONNECTED && event.handle == this->handle_) {
        ESP_LOGI(TAG, "Disconnected");
        this->set_state_(State::STATE_IDLE);
        this->schedule_retry_();
      } else if (this->state_ == State::STATE_CONNECTING && event.handle == this->pending_handle_) {
        // A page timeout or a refused connection
        this->pending_handle_ = 0;
        this->attempt_failed_();
      }
      break;
    case EventType::EVENT_TYPE_WRITE:
      if (event.handle != this->handle_)
        break;
      this->write_pending_ = false;
      this->congested_ = event.value != 0;
      if (event.status != ESP_SPP_SUCCESS) {
        // The block is lost; sending the bytes queued behind it would leave a gap in the stream, so drop the
        // link at once and start a new connection.
        ESP_LOGW(TAG, "Write failed, status %u; reconnecting", event.status);
        this->status_set_warning(LOG_STR("Write failed"));
        this->reset_link_();
      } else {
        this->status_clear_warning();
      }
      break;
    case EventType::EVENT_TYPE_CONGESTION:
      if (event.handle == this->handle_)
        this->congested_ = event.value != 0;
      break;
    case EventType::EVENT_TYPE_PIN_MISSING:
      ESP_LOGW(TAG, "The remote device asks for a PIN, but no 'pin_code' is configured");
      break;
    case EventType::EVENT_TYPE_PASSKEY_REJECTED:
      ESP_LOGW(TAG, "The remote device asks for a passkey, which is not supported");
      break;
    case EventType::EVENT_TYPE_AUTH:
      if (event.status == ESP_BT_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Paired");
      } else {
        ESP_LOGW(TAG, "Pairing failed, status %u", event.status);
      }
      break;
  }
}

// ---------------- UART interface ----------------

void BtSppComponent::write_array(const uint8_t *data, size_t len) {
  if (len == 0)
    return;
  const size_t room = this->available_for_write();
  if (len > room) {
    // Drop the whole write: a partial command is worse than none.
    this->drop_tx_(len);
    return;
  }
  std::memcpy(this->tx_buffer_ + this->tx_len_, data, len);
  this->tx_len_ += len;
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++)
    this->debug_callback_.call(uart::UART_DIRECTION_TX, data[i]);
#endif
  this->send_pending_();
}

void BtSppComponent::drop_tx_(size_t len) {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_drop_log_ != 0 && now - this->last_drop_log_ < DROP_LOG_INTERVAL_MS)
    return;
  this->last_drop_log_ = now;
  ESP_LOGW(TAG, "%s, dropped %u bytes",
           this->is_connected() ? LOG_STR_LITERAL("TX buffer full") : LOG_STR_LITERAL("Not connected"),
           static_cast<unsigned>(len));
}

void BtSppComponent::send_pending_() {
  if (this->tx_len_ == 0 || this->write_pending_ || this->congested_ || !this->is_connected())
    return;
  const size_t len = std::min(this->tx_len_, MAX_WRITE_SIZE);
  // Bluedroid copies the data before esp_spp_write() returns.
  esp_err_t err = esp_spp_write(this->handle_, static_cast<int>(len), this->tx_buffer_);
  if (err != ESP_OK) {
    // Keep the bytes; the next loop tries again. Logged once until a write succeeds.
    if (!this->status_has_warning()) {
      ESP_LOGW(TAG, "esp_spp_write failed: %s", esp_err_to_name(err));
    }
    this->status_set_warning(LOG_STR("Write failed"));
    return;
  }
  this->write_pending_ = true;
  this->tx_len_ -= len;
  if (this->tx_len_ != 0)
    std::memmove(this->tx_buffer_, this->tx_buffer_ + len, this->tx_len_);
}

size_t BtSppComponent::available_for_write() { return this->is_connected() ? this->tx_size_ - this->tx_len_ : 0; }

size_t BtSppComponent::available() {
  if (this->rx_stream_ == nullptr)
    return 0;
  return xStreamBufferBytesAvailable(this->rx_stream_) + (this->has_peek_ ? 1 : 0);
}

bool BtSppComponent::peek_byte(uint8_t *data) {
  if (!this->has_peek_) {
    if (this->rx_stream_ == nullptr || xStreamBufferReceive(this->rx_stream_, &this->peek_, 1, 0) != 1)
      return false;
    this->has_peek_ = true;
  }
  *data = this->peek_;
  return true;
}

bool BtSppComponent::read_array(uint8_t *data, size_t len) {
  if (len == 0)
    return true;
  if (this->available() < len) {
    // Callers that write a request and read the answer at once expect a short wait, as on a hardware UART.
    if (!this->is_connected())
      return false;
    this->send_pending_();
    if (!this->check_read_timeout_(len))
      return false;
  }
  size_t offset = 0;
  if (this->has_peek_) {
    data[offset++] = this->peek_;
    this->has_peek_ = false;
  }
  // available() counted these bytes and only this task removes any.
  while (offset < len)
    offset += xStreamBufferReceive(this->rx_stream_, data + offset, len - offset, 0);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++)
    this->debug_callback_.call(uart::UART_DIRECTION_RX, data[i]);
#endif
  return true;
}

uart::UARTFlushResult BtSppComponent::flush() {
  if (!this->is_connected())
    return uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED;
  this->send_pending_();
  if (this->tx_len_ != 0)
    return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
  // Bluedroid has a copy of every byte, but does not say when they have left the radio.
  return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

// ---------------- Bluedroid task ----------------

void BtSppComponent::post_event_(EventType type, uint8_t status, uint16_t value, uint32_t handle) {
  const Event event{type, status, value, handle};
  if (xQueueSend(this->events_, &event, 0) != pdTRUE) {
    this->events_dropped_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  App.wake_loop_threadsafe();
}

void BtSppComponent::spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (instance != nullptr)
    instance->on_spp_event_(event, param);
}

void BtSppComponent::gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  if (instance != nullptr)
    instance->on_gap_event_(event, param);
}

void BtSppComponent::on_spp_event_(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
    case ESP_SPP_INIT_EVT:
      this->post_event_(EventType::EVENT_TYPE_INIT, param->init.status);
      break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
      this->post_event_(EventType::EVENT_TYPE_DISCOVERY, param->disc_comp.status,
                        param->disc_comp.scn_num > 0 ? param->disc_comp.scn[0] : 0);
      break;
    case ESP_SPP_CL_INIT_EVT:
      this->post_event_(EventType::EVENT_TYPE_CLIENT_INIT, param->cl_init.status, 0, param->cl_init.handle);
      break;
    case ESP_SPP_OPEN_EVT: {
      if (param->open.status == ESP_SPP_SUCCESS) {
        // Bytes may arrive before loop() sees this event; keep them unless another connection is active.
        uint32_t none = 0;
        this->rx_handle_.compare_exchange_strong(none, param->open.handle, std::memory_order_relaxed);
      }
      this->post_event_(EventType::EVENT_TYPE_OPEN, param->open.status, 0, param->open.handle);
      break;
    }
    case ESP_SPP_CLOSE_EVT: {
      uint32_t handle = param->close.handle;
      this->rx_handle_.compare_exchange_strong(handle, 0, std::memory_order_relaxed);
      this->post_event_(EventType::EVENT_TYPE_CLOSE, param->close.status, 0, param->close.handle);
      break;
    }
    case ESP_SPP_WRITE_EVT:
      this->post_event_(EventType::EVENT_TYPE_WRITE, param->write.status, param->write.cong, param->write.handle);
      break;
    case ESP_SPP_CONG_EVT:
      this->post_event_(EventType::EVENT_TYPE_CONGESTION, param->cong.status, param->cong.cong, param->cong.handle);
      break;
    case ESP_SPP_DATA_IND_EVT: {
      const size_t len = param->data_ind.len;
      if (len == 0 || param->data_ind.handle != this->rx_handle_.load(std::memory_order_relaxed))
        break;
      const size_t sent = xStreamBufferSend(this->rx_stream_, param->data_ind.data, len, 0);
      if (sent < len)
        this->rx_dropped_.fetch_add(len - sent, std::memory_order_relaxed);
      App.wake_loop_threadsafe();
      break;
    }
    default:
      break;
  }
}

void BtSppComponent::on_gap_event_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  // Pairing requests are answered here, as Bluedroid waits for the reply.
  switch (event) {
    case ESP_BT_GAP_PIN_REQ_EVT: {
      esp_bt_pin_code_t pin_code{};
      if (this->pin_code_.empty()) {
        esp_bt_gap_pin_reply(param->pin_req.bda, false, 0, pin_code);
        this->post_event_(EventType::EVENT_TYPE_PIN_MISSING);
        break;
      }
      const size_t len = std::min(this->pin_code_.size(), sizeof(pin_code));
      std::memcpy(pin_code, this->pin_code_.c_str(), len);
      esp_bt_gap_pin_reply(param->pin_req.bda, true, static_cast<uint8_t>(len), pin_code);
      break;
    }
    case ESP_BT_GAP_CFM_REQ_EVT:
      esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
      break;
    case ESP_BT_GAP_KEY_REQ_EVT:
      esp_bt_gap_ssp_passkey_reply(param->key_req.bda, false, 0);
      this->post_event_(EventType::EVENT_TYPE_PASSKEY_REJECTED);
      break;
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      this->post_event_(EventType::EVENT_TYPE_AUTH, param->auth_cmpl.stat);
      break;
    default:
      break;
  }
}

}  // namespace esphome::bt_spp

#endif  // USE_ESP32

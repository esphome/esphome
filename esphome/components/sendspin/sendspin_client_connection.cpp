#include "sendspin_client_connection.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>
#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.client_connection";
static const uint32_t WEBSOCKET_SEND_TIMEOUT_MS = 10;

SendspinClientConnection::SendspinClientConnection(std::string url) : url_(std::move(url)) {}

SendspinClientConnection::~SendspinClientConnection() {
  if (this->client_ != nullptr) {
    esp_websocket_client_stop(this->client_);
    esp_websocket_client_destroy(this->client_);
    this->client_ = nullptr;
  }
}

void SendspinClientConnection::start() {
  if (this->client_ != nullptr) {
    ESP_LOGW(TAG, "Client already started, stopping first");
    esp_websocket_client_stop(this->client_);
    esp_websocket_client_destroy(this->client_);
    this->client_ = nullptr;
  }

  // Configure the websocket client
  esp_websocket_client_config_t config = {};
  config.uri = this->url_.c_str();
  config.disable_auto_reconnect = true;  // We handle reconnection ourselves

  // Create the client
  this->client_ = esp_websocket_client_init(&config);
  if (this->client_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize websocket client");
    return;
  }

  // Register event handler
  esp_websocket_register_events(this->client_, WEBSOCKET_EVENT_ANY, websocket_event_handler, this);

  // Start the client
  esp_err_t err = esp_websocket_client_start(this->client_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start websocket client: %s", esp_err_to_name(err));
    esp_websocket_client_destroy(this->client_);
    this->client_ = nullptr;
    return;
  }

  ESP_LOGD(TAG, "Client connection starting to %s", this->url_.c_str());
}

void SendspinClientConnection::loop() {
  // Handle auto-reconnect
  if (!this->is_connected() && this->auto_reconnect_) {
    uint32_t now = millis();
    if (now - this->last_reconnect_attempt_ > this->reconnect_interval_ms_) {
      this->last_reconnect_attempt_ = now;
      ESP_LOGD(TAG, "Attempting to reconnect to %s", this->url_.c_str());
      this->start();
    }
  }
}

void SendspinClientConnection::disconnect(SendspinGoodbyeReason reason, std::function<void()> on_complete) {
  if (!this->is_connected()) {
    // Not connected - invoke completion callback immediately if provided
    if (on_complete) {
      on_complete();
    }
    return;
  }

  // Send goodbye message and then stop client
  // For client connections, send_text_message is synchronous, so callback fires immediately
  this->send_goodbye_reason(reason, [this, on_complete](bool success, int64_t) {
    // Stop the client regardless of send success
    if (this->client_ != nullptr) {
      esp_websocket_client_stop(this->client_);
    }

    // Invoke user-provided completion callback if provided
    if (on_complete) {
      on_complete();
    }
  });
}

bool SendspinClientConnection::is_connected() const { return this->connected_; }

esp_err_t SendspinClientConnection::send_text_message(const std::string &message, SendCompleteCallback cb) {
  if (!this->is_connected()) {
    // No connection - invoke callback with failure if provided
    if (cb) {
      cb(false, 0);
    }
    return ESP_ERR_INVALID_STATE;
  }

  // esp_websocket_client_send_text is synchronous in the current task
  int sent = esp_websocket_client_send_text(this->client_, message.c_str(), message.length(),
                                            pdMS_TO_TICKS(WEBSOCKET_SEND_TIMEOUT_MS));

  // Capture timestamp after send
  int64_t after_send_time = esp_timer_get_time();

  bool success = (sent >= 0);

  // Invoke callback if provided
  if (cb) {
    cb(success, after_send_time);
  }

  if (!success) {
    ESP_LOGE(TAG, "Failed to send text message (timeout or error): %d", sent);
    return ESP_FAIL;
  }

  return ESP_OK;
}

void SendspinClientConnection::websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                                       void *event_data) {
  // Capture receive time immediately for accurate time synchronization
  int64_t receive_time = esp_timer_get_time();

  SendspinClientConnection *conn = static_cast<SendspinClientConnection *>(handler_args);

  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      conn->handle_connected_();
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
      conn->handle_disconnected_();
      break;
    case WEBSOCKET_EVENT_DATA:
      conn->handle_data_(static_cast<esp_websocket_event_data_t *>(event_data), receive_time);
      break;
    case WEBSOCKET_EVENT_ERROR:
      conn->handle_error_();
      break;
    default:
      break;
  }
}

void SendspinClientConnection::handle_connected_() {
  ESP_LOGD(TAG, "WebSocket connected to %s", this->url_.c_str());
  this->connected_ = true;

  // Invoke the on_connected callback if set (hub uses this to initiate hello handshake)
  if (this->on_connected) {
    this->on_connected(this);
  }
}

void SendspinClientConnection::handle_disconnected_() {
  ESP_LOGD(TAG, "WebSocket disconnected from %s", this->url_.c_str());
  this->connected_ = false;
  this->client_hello_sent_ = false;
  this->server_hello_received_ = false;
  this->pending_time_message_ = false;
  this->reset_websocket_payload_();

  // Invoke the disconnected callback if set
  if (this->on_disconnected) {
    this->on_disconnected(this);
  }
}

void SendspinClientConnection::handle_data_(const esp_websocket_event_data_t *data, int64_t receive_time) {
  if (data == nullptr) {
    return;
  }

  // Determine frame type: text (0x01), binary (0x02), or continuation (0x00)
  if (data->op_code == 0x01 || data->op_code == 0x02) {
    // First frame of a new message - remember the type for continuation frames
    this->is_text_frame_ = (data->op_code == 0x01);
  } else if (data->op_code != 0x00) {
    // Control frames (ping 0x09, pong 0x0A, close 0x08) - ignore
    return;
  }

  // Copy data from ESP-IDF's internal buffer into our payload buffer.
  // On the first chunk of a frame, allocate for the full frame payload so subsequent chunks
  // write into the existing buffer without reallocation.
  if (data->data_len > 0) {
    size_t prepare_len = (data->payload_offset == 0) ? data->payload_len : data->data_len;
    uint8_t *dest = this->prepare_receive_buffer_(prepare_len);
    if (dest == nullptr) {
      ESP_LOGE(TAG, "Allocation failed, dropping connection");
      this->handle_disconnected_();
      return;
    }
    std::memcpy(dest, data->data_ptr, data->data_len);
    this->commit_receive_buffer_(data->data_len);
  }

  // A complete message requires both:
  // 1. FIN flag set (last WebSocket protocol frame of the message)
  // 2. All data for this frame received (handles ESP-IDF buffer-level fragmentation,
  //    where a single frame's payload is delivered across multiple events)
  if (data->fin && (data->payload_offset + data->data_len >= data->payload_len)) {
    this->dispatch_completed_message_(this->is_text_frame_, receive_time);
  }
}

void SendspinClientConnection::handle_error_() {
  ESP_LOGE(TAG, "WebSocket error on connection to %s", this->url_.c_str());
  // Error will typically be followed by a disconnect event
}

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32

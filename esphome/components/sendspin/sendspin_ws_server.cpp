#include "sendspin_ws_server.h"

#ifdef USE_ESP32

#include "sendspin_hub.h"
#include "sendspin_server_connection.h"

#include "esphome/core/log.h"

#include <esp_timer.h>
#include "lwip/sockets.h"  // for close()

namespace esphome {
namespace sendspin {

/*
 * SendspinWsServer manages the HTTP server (httpd) that accepts incoming WebSocket connections.
 *
 * Key Design Points:
 * - The server listener ACCEPTS connections but doesn't OWN them long-term
 * - When a client connects, it creates a SendspinServerConnection and hands it to the hub
 * - The hub decides whether to keep or reject the connection (for handoff logic)
 * - Supports max_connections=2 by default to enable handoff protocol:
 *   - One active connection is managed by the hub
 *   - A second connection can be accepted temporarily during handoff
 *   - The hub completes the handshake and decides which to keep
 *
 * Lifecycle:
 * 1. Hub calls start() with callbacks and configuration
 * 2. Server listens on port 8928 at /sendspin
 * 3. open_callback() creates SendspinServerConnection instances
 * 4. Hub receives connection via new_connection_callback
 * 5. Hub manages connection ownership and handoff logic
 * 6. close_callback() handles socket cleanup
 */

static const char *const TAG = "sendspin.ws_server";

SendspinWsServer::~SendspinWsServer() { this->stop(); }

bool SendspinWsServer::start(SendspinHub *hub, bool task_stack_in_psram, unsigned task_priority) {
  if (this->server_ != nullptr) {
    ESP_LOGW(TAG, "Server already started");
    return true;
  }

  this->hub_ = hub;

  // Configure the HTTP server
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (task_stack_in_psram) {
    config.task_caps = MALLOC_CAP_SPIRAM;
  }
  config.task_priority = task_priority;
  config.server_port = 8928;
  config.max_open_sockets = this->max_connections_;
  config.open_fn = SendspinWsServer::open_callback;
  config.close_fn = SendspinWsServer::close_callback;
  config.global_user_ctx = (void *) this;
  config.global_user_ctx_free_fn = nullptr;
  config.ctrl_port = ESP_HTTPD_DEF_CTRL_PORT + 1;  // Avoid conflict with web_server component

  // Start the HTTP server
  ESP_LOGI(TAG, "Starting server on port: %d (max connections: %d)", config.server_port, this->max_connections_);
  esp_err_t err = httpd_start(&this->server_, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting server: %s", esp_err_to_name(err));
    return false;
  }

  // Register the WebSocket handler
  const httpd_uri_t sendspin_ws_uri = {.uri = "/sendspin",
                                       .method = HTTP_GET,
                                       .handler = SendspinWsServer::websocket_handler,
                                       .user_ctx = (void *) this,
                                       .is_websocket = true};

  err = httpd_register_uri_handler(this->server_, &sendspin_ws_uri);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error registering URI handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    this->server_ = nullptr;
    return false;
  }

  return true;
}

void SendspinWsServer::stop() {
  if (this->server_ != nullptr) {
    ESP_LOGD(TAG, "Stopping server");
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }
}

esp_err_t SendspinWsServer::open_callback(httpd_handle_t handle, int sockfd) {
  ESP_LOGD(TAG, "New client connection on socket %d", sockfd);

  SendspinWsServer *server = (SendspinWsServer *) httpd_get_global_user_ctx(handle);
  if (server == nullptr) {
    ESP_LOGE(TAG, "Server context is null in open_callback");
    return ESP_FAIL;
  }

  // Create a new connection instance
  auto connection = std::make_unique<SendspinServerConnection>(handle, sockfd);

  // Notify the hub of the new connection (hub decides whether to keep it)
  if (server->new_connection_callback_) {
    server->new_connection_callback_(std::move(connection));
  } else {
    ESP_LOGW(TAG, "No new connection callback set, connection will be dropped");
  }

  return ESP_OK;
}

void SendspinWsServer::close_callback(httpd_handle_t handle, int sockfd) {
  ESP_LOGD(TAG, "Client closed connection on socket %d", sockfd);

  SendspinWsServer *server = (SendspinWsServer *) httpd_get_global_user_ctx(handle);

  // Notify the hub so it can identify and clean up the connection
  if (server != nullptr && server->connection_closed_callback_) {
    server->connection_closed_callback_(sockfd);
  }

  // Close the socket
  close(sockfd);
}

esp_err_t SendspinWsServer::websocket_handler(httpd_req_t *req) {
  // Capture timestamp immediately for accurate time synchronization
  int64_t receive_time = esp_timer_get_time();

  SendspinWsServer *server = (SendspinWsServer *) req->user_ctx;

  // Handle WebSocket handshake (HTTP_GET)
  if (req->method == HTTP_GET) {
    // Find the connection and invoke its on_connected callback
    int sockfd = httpd_req_to_sockfd(req);
    SendspinServerConnection *conn = nullptr;
    if (server->find_connection_callback_) {
      conn = server->find_connection_callback_(sockfd);
    }
    if (conn != nullptr && conn->on_connected) {
      conn->on_connected(conn);
    }
    return ESP_OK;
  }

  // Find connection by sockfd
  int sockfd = httpd_req_to_sockfd(req);
  SendspinServerConnection *conn = nullptr;
  if (server->find_connection_callback_) {
    conn = server->find_connection_callback_(sockfd);
  }

  if (conn == nullptr) {
    ESP_LOGE(TAG, "No connection found for sockfd %d", sockfd);
    return ESP_FAIL;
  }

  // Delegate to connection's handle_data
  return conn->handle_data(req, receive_time);
}

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32

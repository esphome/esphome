#include "async_tcp_socket.h"

#if !defined(USE_ESP32) && !defined(USE_ESP8266) && !defined(USE_RP2) && !defined(USE_LIBRETINY) && \
    (defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || defined(USE_SOCKET_IMPL_BSD_SOCKETS))

#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include <cerrno>
#include <sys/select.h>

namespace esphome::async_tcp {

static const char *const TAG = "async_tcp";

// Read buffer size matches TCP MSS (1500 MTU - 40 bytes IP/TCP headers).
// This implementation only runs on ESP-IDF and host which have ample stack.
static constexpr size_t READ_BUFFER_SIZE = 1460;

bool AsyncClient::connect(const char *host, uint16_t port) {
  if (connected_ || connecting_) {
    ESP_LOGW(TAG, "Already connected/connecting");
    return false;
  }

  // Resolve address
  struct sockaddr_storage addr;
  socklen_t addrlen = esphome::socket::set_sockaddr((struct sockaddr *) &addr, sizeof(addr), host, port);
  if (addrlen == 0) {
    ESP_LOGE(TAG, "Invalid address: %s", host);
    if (error_cb_)
      error_cb_(error_arg_, this, -1);
    return false;
  }

  // Create socket with loop monitoring
  int family = ((struct sockaddr *) &addr)->sa_family;
  socket_ = esphome::socket::socket_loop_monitored(family, SOCK_STREAM, IPPROTO_TCP);
  if (!socket_) {
    ESP_LOGE(TAG, "Failed to create socket");
    if (error_cb_)
      error_cb_(error_arg_, this, -1);
    return false;
  }

  if (socket_->setblocking(false) != 0) {
    // A blocking connect()/read() would stall the whole loop
    ESP_LOGE(TAG, "Failed to set nonblocking: errno %d", errno);
    socket_.reset();
    if (error_cb_)
      error_cb_(error_arg_, this, errno);
    return false;
  }

  int err = socket_->connect((struct sockaddr *) &addr, addrlen);
  if (err == 0) {
    // Connection succeeded immediately (rare, but possible for localhost)
    connected_ = true;
    if (connect_cb_)
      connect_cb_(connect_arg_, this);
    return true;
  }
  const int saved_errno = errno;
  if (saved_errno != EINPROGRESS) {
    ESP_LOGE(TAG, "Connect failed: %d", saved_errno);
    close();
    if (error_cb_)
      error_cb_(error_arg_, this, saved_errno);
    return false;
  }

  connecting_ = true;
  return true;
}

void AsyncClient::close() {
  socket_.reset();
  bool was_connected = connected_;
  connected_ = false;
  connecting_ = false;
  if (was_connected && disconnect_cb_)
    disconnect_cb_(disconnect_arg_, this);
}

size_t AsyncClient::write(const char *data, size_t len) {
  if (!socket_ || !connected_)
    return 0;

  ssize_t sent = socket_->write(data, len);
  if (sent < 0) {
    const int err = errno;
    if (err != EAGAIN && err != EWOULDBLOCK) {
      ESP_LOGE(TAG, "Write error: %d", err);
      close();
      if (error_cb_)
        error_cb_(error_arg_, this, err);
    }
    return 0;
  }
  return sent;
}

void AsyncClient::loop() {
  if (!socket_)
    return;

  if (connecting_) {
    int err = 0;
    switch (socket::poll_connect(*socket_, err)) {
      case socket::ConnectPollResult::CONNECT_POLL_PENDING:
        break;
      case socket::ConnectPollResult::CONNECT_POLL_CONNECTED:
        connecting_ = false;
        connected_ = true;
        if (connect_cb_)
          connect_cb_(connect_arg_, this);
        break;
      case socket::ConnectPollResult::CONNECT_POLL_ERROR:
        ESP_LOGW(TAG, "Connection failed: %d", err);
        close();
        if (error_cb_)
          error_cb_(error_arg_, this, err);
        break;
    }
  } else if (connected_) {
    // For connected sockets, use the Application's select() results
    if (!socket_->ready())
      return;

    uint8_t buf[READ_BUFFER_SIZE];
    ssize_t len = socket_->read(buf, READ_BUFFER_SIZE);

    if (len == 0) {
      ESP_LOGI(TAG, "Connection closed by peer");
      close();
    } else if (len > 0) {
      if (data_cb_)
        data_cb_(data_arg_, this, buf, len);
    } else {
      const int err = errno;
      if (err != EAGAIN && err != EWOULDBLOCK) {
        ESP_LOGW(TAG, "Read error: %d", err);
        close();
        if (error_cb_)
          error_cb_(error_arg_, this, err);
      }
    }
  }
}

}  // namespace esphome::async_tcp

#endif

#include "async_tcp_impl.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_TCP_ASYNC_TCP

namespace esphome {
namespace tcp {

static const char *TAG = "tcp.async_tcp";
static const char *TAG_SERVER = "tcp.async_tcp_server";

#define SOCKET_LOGVV(format, ...) ESP_LOGVV(TAG, "%s:%u: " format, this->host_.c_str(), this->get_remote_port(), ##__VA_ARGS__)
#define SOCKET_LOGV(format, ...) ESP_LOGV(TAG, "%s:%u: " format, this->host_.c_str(), this->get_remote_port(), ##__VA_ARGS__)
#define SOCKET_LOG(format, ...) ESP_LOGD(TAG, "%s:%u: " format, this->host_.c_str(), this->get_remote_port(), ##__VA_ARGS__)
#define SOCKET_SERVER_LOGVV(format, ...) ESP_LOGVV(TAG_SERVER, format, ##__VA_ARGS__)

bool AsyncTCPImpl::read(uint8_t *destination_buffer, size_t size) {
  size_t available = this->available();
  if (size > available) {
    SOCKET_LOG("Requested read() with size %u when only %u bytes are available! This call will now block!",
               size, available);
  }
  SOCKET_LOGVV("read(dest=%p, size=%u) avail=%u", destination_buffer, size, available);

  for (size_t at = 0; at < size; ) {
    while (this->available() == 0) {
      // Waiting for data, blocking call!
      if (!this->is_readable()) {
        SOCKET_LOG("read() Socket closed while reading blocking data!");
        return false;
      }
      yield();
    }
    auto pair = this->rx_buffer_.pop_front_linear(size - at);
    memcpy(destination_buffer + at, pair.first, pair.second);
    at += pair.second;
  }
  return true;
}
bool AsyncTCPImpl::write(const uint8_t *buffer, size_t size) {
  if (buffer == nullptr)
    return false;

  this->drain_reserve_buffer_();
  if (size > this->available_for_write()) {
    SOCKET_LOG("write: Attempted to write more than allocated! size=%u available_for_write=%u",
               size, this->available_for_write());
    // push_back will handle auto-resizing
  }

  SOCKET_LOGVV("write(%p, %u)", buffer, size);

  if (this->reserve_buffer_.empty()) {
    // reserve buffer is empty, write directly to TCP
    size_t sendbuf_size = this->client_->space();
    size_t to_write = std::min(sendbuf_size, size);
    bool has_more = to_write != size;
    size_t written = this->write_internal_(buffer, to_write, !has_more);
    if (written != to_write) {
      // writing failed, overwrite to_write to instead put it in the reserve buffer
      to_write = written;
    }

    if (has_more) {
      // write rest in reserve buffer
      SOCKET_LOG("  reserve_buffer.push_back(%p, %u)", buffer + to_write, size - to_write);
      this->reserve_buffer_.push_back(buffer + to_write, size - to_write);
    }
  } else {
    // reserve buffer has data, write to reserve buffer
    SOCKET_LOG("  reserve_buffer.push_back(%p, %u)", buffer, size);
    this->reserve_buffer_.push_back(buffer, size);
  }

  return true;
}
size_t AsyncTCPImpl::write_internal_(const uint8_t *buffer, size_t size, bool psh_flag) {
  // can't log in write methods because that would
  SOCKET_LOGVV("  write_internal_(%p, size=%u, PSH=%s)", buffer, size, YESNO(psh_flag));
  if (size == 0)
    return 0;
  const size_t sendbuf = this->client_->space();

  uint8_t flags = ASYNC_WRITE_FLAG_COPY;
  if (!psh_flag) {
    flags |= ASYNC_WRITE_FLAG_MORE;
  }

  size_t written = this->client_->add(reinterpret_cast<const char *>(buffer), size, flags);
  if (written != size) {
    SOCKET_LOGVV("  Error: only %u written", written);
  }
  // TODO: maybe don't send immediately?
  // this->client_->send();
  return written;
}

void AsyncTCPImpl::on_connect_cb_() {
  SOCKET_LOGVV("on_connect()");
  this->connect_called_ = false;
  this->remote_port_ = this->client_->remotePort();
}
void AsyncTCPImpl::on_disconnect_cb_() {
  // called in AsyncClient::_close
  SOCKET_LOGVV("on_disconnect()");
  // nothing to do here
}
void AsyncTCPImpl::on_ack_cb_(size_t len, uint32_t time) {
  // printing here would create make any log line create infinite ACKs
  // SOCKET_LOGVV("on_ack(len=%u, time=%u)", len, time);
  // nothing to do here
}
void AsyncTCPImpl::on_error_cb_(int8_t error) {
  SOCKET_LOGVV("on_error(%d): %s", error, this->client_->errorToString(error));
}
void AsyncTCPImpl::on_data_cb_(uint8_t *data, size_t len) {
  SOCKET_LOGVV("on_data(%p, len=%u)", data, len);
  if (len == 0 || data == nullptr)
    return;
  this->rx_buffer_.push_back(data, len);
}
void AsyncTCPImpl::on_timeout_cb_(uint32_t time) {
  SOCKET_LOGVV("on_timeout(%d)", time);
  // nothing to do here
}
bool AsyncTCPImpl::drain_reserve_buffer_() {
  while (!this->reserve_buffer_.empty()) {
    // Loop until sendbuffer is full
    size_t sendbuf_size = this->client_->space();
    if (sendbuf_size == 0)
      return false;
    SOCKET_LOGVV("drain_reserve_buffer_ sendbuf=%u size=%u", sendbuf_size, this->reserve_buffer_.size());
    auto pair = this->reserve_buffer_.peek_front_linear(sendbuf_size);
    size_t ret = this->write_internal_(pair.first, pair.second, !this->reserve_buffer_.empty());
    this->client_->send();
    this->reserve_buffer_.pop_front_linear(ret);
    if (ret != pair.second) {
      // Writing failed, stop draining.
      return false;
    }
  }
  return true;
}

bool AsyncTCPImpl::flush() {
  uint32_t start_time = millis();
  while (true) {
    uint32_t now = millis();
    if (now - start_time > 50)
      // wait a maximum of 50ms (could be done better)
      return false;

    this->client_->send();
    this->drain_reserve_buffer_();
    size_t sendbuf = this->client_->space();
    // This check appears to work and be used in other places as well
    // (ClientContext in eps8266 Arduino).
    if (sendbuf == TCP_SND_BUF)
      return true;
  }
}

void AsyncTCPServerImpl::on_accept_cb_(AsyncClient *client) {
  SOCKET_SERVER_LOGVV("on_accept(%s:%d)", client->remoteIP().toString().c_str(), client->remotePort());
  this->accepted_.push(new AsyncTCPImpl(client));
}

void AsyncTCPImpl::loop() {
  if (this->is_writable()) {
    this->client_->send();
    this->drain_reserve_buffer_();
  }
  State state = this->state();
  if (state != this->last_state_) {
    SOCKET_LOGVV("New state: %s", socket_state_to_string(state));
    this->last_state_ = state;
  }
}

}  // namespace tcp
}  // namespace esphome

#endif // USE_TCP_ASYNC_TCP

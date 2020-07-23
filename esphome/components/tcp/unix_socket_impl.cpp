#include "esphome/core/defines.h"
#ifdef USE_TCP_UNIX_SOCKET

#include "unix_socket_impl.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
#include <lwip/sockets.h>
#define connect lwip_connect_r
#define close lwip_close_r
#define recv lwip_recv_r
#define ioctl lwip_ioctl_r
#define send lwip_send_r
#define socket lwip_socket
#define bind lwip_bind_r
#define listen lwip_listen
#define accept lwip_accept_r
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

namespace esphome {
namespace tcp {

static const char *TAG = "unix_tcp";
static const char *TAG_SERVER = "unix_tcp_server";

#define SOCKET_LOGVV(format, ...) ESP_LOGV(TAG, "%s (%d): " format, this->host_.c_str(), this->fd_, ##__VA_ARGS__)
#define SOCKET_LOGV(format, ...) ESP_LOGV(TAG, "%s (%d): " format, this->host_.c_str(), this->fd_, ##__VA_ARGS__)
#define SOCKET_LOG(format, ...) ESP_LOGD(TAG, "%s (%d): " format, this->host_.c_str(), this->fd_, ##__VA_ARGS__)
#define SOCKET_SERVER_LOG(format, ...) ESP_LOGD(TAG_SERVER, "%d: " format, this->fd_, ##__VA_ARGS__)

bool UnixSocketImpl::connect(IPAddress ip, uint16_t port) {
  this->host_ = ip.toString().c_str();
  return this->connect_(ip, port);
}
bool UnixSocketImpl::connect_(IPAddress ip, uint16_t port) {
  this->fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (this->fd_ < 0) {
    SOCKET_LOG("socket() failed! %s", strerror(errno));
    return false;
  }

  uint8_t file_flags = fcntl(this->fd_, F_GETFL, 0);
  fcntl(this->fd_, F_SETFL, file_flags | O_NONBLOCK);

  const uint32_t ip_addr32 = ip;
  sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  memcpy(&server_address.sin_addr.s_addr, &ip_addr32, sizeof(ip_addr32));
  server_address.sin_port = htons(port);

  int res = connect(this->fd_, reinterpret_cast<sockaddr *>(&server_address), sizeof(server_address));
  if (res < 0 && errno != EINPROGRESS) {
    SOCKET_LOG("connect(%d) failed: %s", this->fd_, strerror(errno));
    this->abort_();
    return false;
  }

  this->is_connecting_ = true;
  this->timeout_started_ = millis();
  return true;
}
void UnixSocketImpl::abort_() {
  if (this->fd_ < 0)
    return;

  close(this->fd_);
  this->is_connecting_ = false;
  this->is_connected_ = false;
  this->has_aborted_ = true;
}

void UnixSocketImpl::loop() {
  this->check_connecting_finished_();
  this->drain_reserve_buffer_();

  this->update_available_for_write_();
  this->check_disconnected_();
}
void UnixSocketImpl::update_available_for_write_() {
  if (!this->is_connected_)
    return;
  const uint32_t now = millis();
  // assume a constant 20kB/s transfer rate as an approximation
  uint32_t dt = now - this->last_loop_;
  this->available_for_write_ += dt * 20;
  this->available_for_write_ = std::min(this->available_for_write_, (size_t) TCP_SND_BUF);
  if (!this->reserve_buffer_.empty())
    this->available_for_write_ = 0;
  this->last_loop_ = now;
}
void UnixSocketImpl::check_connecting_finished_() {
  if (!this->is_connecting_)
    return;
  fd_set write_set{};
  FD_SET(this->fd_, &write_set);
  timeval tv{};
  int res = select(this->fd_ + 1, nullptr, &write_set, nullptr, &tv);
  if (res < 0) {
    SOCKET_LOG("select() after connect() failed: %d %s", res, strerror(errno));
    this->abort_();
    return;
  } else if (res == 0) {
    uint32_t now = millis();
    if (now - this->timeout_started_ > this->timeout_) {
      SOCKET_LOG("connecting timed out");
      this->abort_();
      return;
    }
  }
  // success
  assert(FD_ISSET(this->fd_, &write_set));
  int sockerr;
  socklen_t len = sizeof(int);
  res = getsockopt(this->fd_, SOL_SOCKET, SO_ERROR, &sockerr, &len);

  if (res < 0) {
    SOCKET_LOG("getsockopt() failed: %d %s", res, strerror(errno));
    this->abort_();
    return;
  }

  if (sockerr != 0) {
    SOCKET_LOG("socket error: %d %s", sockerr, strerror(sockerr));
    this->abort_();
    return;
  }

  this->is_connecting_ = false;
  this->is_connected_ = true;
  this->available_for_write_ = TCP_SND_BUF;
  this->last_loop_ = millis();
  SOCKET_LOGV("Connected successfully!");
}
void UnixSocketImpl::check_disconnected_() {
  if (!this->is_connected_)
    return;
  uint8_t dummy;
  recv(this->fd_, &dummy, 0, 0);
  switch (errno) {
    case EWOULDBLOCK:
    case ENOENT:
      break;
    case ENOTCONN:
    case EPIPE:
    case ECONNRESET:
    case ECONNREFUSED:
    case ECONNABORTED:
      this->abort_();
      SOCKET_LOG("Disconnected: %s", strerror(errno));
      break;
  }
}
size_t UnixSocketImpl::available() {
  int count;
  int res = ioctl(this->fd_, FIONREAD, &count);
  if (res < 0) {
    SOCKET_LOG("ioctl(FIONREAD) failed: %d %s", res, strerror(res));
    this->abort_();
    return 0;
  }
  return count;
}
void UnixSocketImpl::read(uint8_t *buffer, size_t size) {
  size_t available = this->available();
  if (size > available) {
    SOCKET_LOG("Requested read() with size %u when only %u bytes are available! This call will now block!",
               size, available);
  }

  size_t to_read = size;
  while (to_read != 0) {
    while (this->available() == 0) {
      // Waiting for data, blocking call!
      if (!this->is_readable()) {
        SOCKET_LOG("read() Socket closed while reading blocking data!");
        return;
      }
      yield();
    }

    int res = recv(this->fd_, buffer, to_read, 0);
    if (res < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // even if we check if the data's available, somethines recv() thinks
        // otherwise. Block in that case
        SOCKET_LOGVV("recv(%u) wants to block: %s", to_read, strerror(errno));
        continue;
      }
      SOCKET_LOG("recv(%u) failed: %s", to_read, strerror(errno));
      this->abort_();
      return;
    } else if (res == 0) {
      // "For TCP sockets, the return value 0 means the peer has closed its half side of the connection."
      SOCKET_LOG("recv(%u) TCP closed!", to_read);
      this->abort_();
      return;
    }

    to_read -= res;
    buffer += res;
  }
}
void UnixSocketImpl::write(const uint8_t *buffer, size_t size) {
  if (size == 0)
    return;
  this->drain_reserve_buffer_();
  size_t written = 0;
  if (this->reserve_buffer_.empty()) {
    // if the reserve buffer is empty, we can write directly to the socket
    written += this->write_internal_(buffer, size);
  }
  if (written == size)
    // already wrote everything we wanted, return
    return;

  // put rest into the reserve buffer
  size_t left = size - written;
  this->reserve_buffer_.push_back(buffer + written, left);
}

void UnixSocketImpl::flush() {
  // not implemented
}
void UnixSocketImpl::close(bool force) {
  close(this->fd_);
}
tcp::TCPSocket::State UnixSocketImpl::state() {
  if (this->has_aborted_)
    return TCPSocket::STATE_CLOSED;
  if (this->is_connecting_)
    return TCPSocket::STATE_CONNECTING;
  if (this->is_connected_)
    return TCPSocket::STATE_CONNECTED;
  // should not happen
  return TCPSocket::STATE_CLOSING;
}
std::string UnixSocketImpl::get_host() {
  return this->host_;
}
size_t UnixSocketImpl::available_for_write() {
  return this->available_for_write_;
}
UnixSocketImpl::~UnixSocketImpl() {
  this->close(true);
}
size_t UnixSocketImpl::write_internal_(const uint8_t *buffer, size_t size) {
  int ret = send(this->fd_, buffer, size, 0);
  if (ret != size) {
    this->available_for_write_ = 0;
    this->last_loop_ = millis();
  } else if (ret >= 0) {
    this->available_for_write_ -= ret;
  }

  if (ret >= 0)
    return ret;

  // failed, check errno
  if (errno == EAGAIN || errno == EWOULDBLOCK)
    return 0;

  SOCKET_LOG("socket error on send(size=%u): %s", size, strerror(errno));
  this->abort_();
  return 0;
}
void UnixSocketImpl::drain_reserve_buffer_() {
  while (!this->reserve_buffer_.empty() && this->is_connected() && this->can_write_()) {
    auto pair = this->reserve_buffer_.peek_front_linear(this->reserve_buffer_.capacity());
    size_t written = this->write_internal_(pair.first, pair.second);
    if (written > 0)
      this->reserve_buffer_.pop_front_linear(written);
  }
}
bool UnixSocketImpl::can_write_() {
  fd_set write_set{};
  FD_SET(this->fd_, &write_set);
  timeval tv{};
  int res = select(this->fd_ + 1, nullptr, &write_set, nullptr, &tv);
  return res == 1 && FD_ISSET(this->fd_, &write_set);
}
bool UnixSocketImpl::can_read_() {
  fd_set read_set{};
  FD_SET(this->fd_, &read_set);
  timeval tv{};
  int res = select(this->fd_ + 1, &read_set, nullptr, nullptr, &tv);
  return res == 1 && FD_ISSET(this->fd_, &read_set);
}
}
void UnixSocketImpl::reserve_at_least(size_t size) {
  if (size <= TCP_SND_BUF)
    return;
  if (size - TCP_SND_BUF > this->reserve_buffer_.capacity()) {
    SOCKET_LOGVV("reserve_at_least(%u) capacity=%u", size, this->reserve_buffer_.capacity());
  }
  this->reserve_buffer_.reserve(size - TCP_SND_BUF);
}
void UnixSocketImpl::ensure_capacity(size_t size) {
  size_t avail = this->available_for_write();
  if (size <= avail)
    return;
  if (size - avail > this->reserve_buffer_.capacity()) {
    SOCKET_LOGVV("ensure_capacity(%u) avail=%u capacity=%u", size, avail, this->reserve_buffer_.capacity());
  }
  this->reserve_buffer_.reserve(size - avail);
}
IPAddress UnixSocketImpl::get_remote_address() {
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);
  getpeername(this->fd_, (sockaddr*)&addr, &len);
  auto *s = (sockaddr_in *) &addr;
  return IPAddress(uint32_t(s->sin_addr.s_addr));
}
uint16_t UnixSocketImpl::get_remote_port() {
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);
  getpeername(this->fd_, (sockaddr*)&addr, &len);
  auto *s = (sockaddr_in *) &addr;
  return ntohs(s->sin_port);
}
IPAddress UnixSocketImpl::get_local_address() {
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);
  getsockname(this->fd_, (sockaddr*)&addr, &len);
  auto *s = (sockaddr_in *) &addr;
  return IPAddress(uint32_t(s->sin_addr.s_addr));
}
uint16_t UnixSocketImpl::get_local_port() {
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);
  getsockname(this->fd_, (sockaddr*)&addr, &len);
  auto *s = (sockaddr_in *) &addr;
  return ntohs(s->sin_port);
}
void UnixSocketImpl::set_no_delay(bool no_delay) {
  int flag = no_delay;
  setsockopt(this->fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}
void UnixSocketImpl::set_timeout(uint32_t timeout_ms) {
  this->timeout_ = timeout_ms;
}
bool UnixSocketImpl::connect(const std::string &host, uint16_t port) {
  // TODO
  return false;
}
UnixSocketImpl::UnixSocketImpl(int fd) {
  this->fd_ = fd;
  this->is_connected_ = true;
  this->host_ = this->get_remote_address().toString().c_str();
  this->last_loop_ = millis();
}

bool UnixServerImpl::bind(uint16_t port) {
  this->fd_ = (AF_INET, SOCK_STREAM, 0);
  if (this->fd_ < 0) {
    SOCKET_SERVER_LOG("socket() failed: %d %s", this->fd_, strerror(errno));
    return false;
  }
  sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(port);
  int res = bind(this->fd_, reinterpret_cast<const sockaddr *>(&server), sizeof(server));
  if (res < 0) {
    SOCKET_SERVER_LOG("bind() failed: %s", strerror(errno));
    return false;
  }
  const uint8_t max_clients = 4;
  res = listen(this->fd_, max_clients);
  if (res < 0) {
    SOCKET_SERVER_LOG("bind() failed: %s", strerror(errno));
    return false;
  }

  uint8_t file_flags = fcntl(this->fd_, F_GETFL, 0);
  fcntl(this->fd_, F_SETFL, file_flags | O_NONBLOCK);
  return true;
}
std::unique_ptr<TCPSocket> UnixServerImpl::accept() {
  sockaddr_in client{};
  int cs = sizeof(sockaddr_in);
  int client_fd = accept(this->fd_, (sockaddr *) &client, (socklen_t *) &cs);
  if (client_fd < 0)
    return std::unique_ptr<TCPSocket>();
  return std::unique_ptr<UnixSocketImpl>(new UnixSocketImpl(client_fd));
}
void UnixServerImpl::close(bool force) {
  close(this->fd_);
  this->fd_ = -1;
}

}  // namespace tcp
}  // namespace tcp

#endif  // USE_TCP_UNIX_SOCKET

#include "tcp_uart.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#ifdef USE_ESP32
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#else
#include <netdb.h>
#endif

namespace esphome {
namespace tcp_uart {

static const char *const TAG = "tcp_uart";

uint32_t loop_time() { return App.get_loop_component_start_time(); }

float TcpUart::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void TcpUart::setup() {
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void TcpUart::dump_config() {
  ESP_LOGCONFIG(TAG, "TCP UART:");
  ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
}

void TcpUart::on_shutdown() { this->close_sock_(); }

void TcpUart::set_link_up_(bool up) {
  if (this->connected_ == up) {
    return;
  }
  this->connected_ = up;
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void TcpUart::close_sock_() {
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connecting_ = false;
  this->set_link_up_(false);
  this->rx_.clear();
  this->tx_.clear();
}

void TcpUart::apply_socket_options_(socket::Socket *sock) {
  int yes = 1;
  sock->setblocking(false);
  sock->setsockopt(IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
  sock->setsockopt(SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
  int idle = 30;
  int interval = 10;
  int count = 3;
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
  sock->setsockopt(IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
}

#ifdef USE_ESP32
void TcpUart::dns_found_(const char *name, const ip_addr_t *addr, void *arg) {
  auto *self = static_cast<TcpUart *>(arg);
  if (addr != nullptr && IP_IS_V4(addr)) {
    self->resolved_addr_.store(ip4_addr_get_u32(ip_2_ip4(addr)));
    self->have_addr_.store(true);
  } else {
    self->resolve_failed_.store(true);
    ESP_LOGW(TAG, "DNS failed for %s", name);
  }
  self->resolving_.store(false);
}
#endif

void TcpUart::try_resolve_() {
  if (this->have_addr_.load() || this->resolving_.load()) {
    return;
  }
  struct sockaddr_storage literal;
  if (socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&literal), sizeof(literal), this->host_.c_str(),
                           this->port_) != 0) {
    snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s", this->host_.c_str());
    this->have_addr_.store(true);
    return;
  }
#ifdef USE_ESP32
  ip_addr_t cached;
  err_t err = dns_gethostbyname(this->host_.c_str(), &cached, &TcpUart::dns_found_, this);
  if (err == ERR_OK && IP_IS_V4(&cached)) {
    this->resolved_addr_.store(ip4_addr_get_u32(ip_2_ip4(&cached)));
    this->have_addr_.store(true);
    return;
  }
  if (err == ERR_INPROGRESS) {
    this->resolving_.store(true);
    return;
  }
#elif defined(USE_HOST)
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res = nullptr;
  if (getaddrinfo(this->host_.c_str(), nullptr, &hints, &res) == 0 && res != nullptr) {
    char buf[INET_ADDRSTRLEN];
    auto *in = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf)) != nullptr) {
      snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s", buf);
      this->have_addr_.store(true);
    }
    freeaddrinfo(res);
    if (this->have_addr_.load()) {
      return;
    }
  }
#endif
  this->resolve_failed_.store(true);
  ESP_LOGW(TAG, "Could not resolve %s", this->host_.c_str());
}

bool TcpUart::ip_ready_() {
  if (this->resolved_ip_[0] != '\0') {
    return true;
  }
  if (!this->have_addr_.load()) {
    return false;
  }
  struct in_addr addr{};
  addr.s_addr = this->resolved_addr_.load();
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr) {
    return false;
  }
  snprintf(this->resolved_ip_, sizeof(this->resolved_ip_), "%s", buf);
  return true;
}

void TcpUart::try_connect_() {
  if (this->sock_ != nullptr || loop_time() < this->next_connect_ms_) {
    return;
  }
  if (this->resolve_failed_.exchange(false)) {
    this->have_addr_.store(false);
    this->resolved_ip_[0] = '\0';
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  this->try_resolve_();
  if (!this->ip_ready_()) {
    return;
  }
  struct sockaddr_storage dest;
  socklen_t dest_len =
      socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest), this->resolved_ip_, this->port_);
  if (dest_len == 0) {
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  this->sock_ = socket::socket(dest.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (this->sock_ == nullptr) {
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  this->apply_socket_options_(this->sock_.get());
  int rc = this->sock_->connect(reinterpret_cast<struct sockaddr *>(&dest), dest_len);
  if (rc == 0 || errno == EINPROGRESS) {
    this->connecting_ = rc != 0;
    this->set_link_up_(rc == 0);
    return;
  }
  this->sock_.reset();
  this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
}

void TcpUart::read_socket_() {
  if (this->sock_ == nullptr) {
    return;
  }
  if (this->connecting_) {
    int err = 0;
    socklen_t len = sizeof(err);
    if (this->sock_->getsockopt(SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
      this->close_sock_();
      this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
      return;
    }
    this->connecting_ = false;
    this->set_link_up_(true);
    ESP_LOGI(TAG, "Connected to %s:%u", this->host_.c_str(), this->port_);
  }
  uint8_t tmp[128];
  ssize_t count = this->sock_->read(tmp, sizeof(tmp));
  if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    ESP_LOGW(TAG, "Connection lost");
    this->close_sock_();
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
    return;
  }
  for (ssize_t i = 0; i < count && this->rx_.size() < 1024; i++) {
    this->rx_.push(tmp[i]);
  }
}

void TcpUart::flush_tx_() {
  if (!this->connected_ || this->sock_ == nullptr || this->tx_.empty()) {
    return;
  }
  ssize_t sent = this->sock_->write(this->tx_.data(), this->tx_.size());
  if (sent > 0) {
    this->tx_.erase(this->tx_.begin(), this->tx_.begin() + sent);
    return;
  }
  if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    ESP_LOGW(TAG, "Send failed");
    this->close_sock_();
    this->next_connect_ms_ = loop_time() + this->reconnect_interval_ms_;
  }
}

void TcpUart::loop() {
  this->try_connect_();
  this->read_socket_();
  this->flush_tx_();
}

void TcpUart::write_array(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len && this->tx_.size() < 1024; i++) {
    this->tx_.push_back(data[i]);
  }
}

bool TcpUart::peek_byte(uint8_t *data) {
  if (this->rx_.empty()) {
    return false;
  }
  *data = this->rx_.front();
  return true;
}

bool TcpUart::read_array(uint8_t *data, size_t len) {
  if (this->rx_.size() < len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    data[i] = this->rx_.front();
    this->rx_.pop();
  }
  return true;
}

size_t TcpUart::available() { return this->rx_.size(); }

uart::UARTFlushResult TcpUart::flush() {
  this->flush_tx_();
  if (this->tx_.empty()) {
    return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
  }
  return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
}

}  // namespace tcp_uart
}  // namespace esphome

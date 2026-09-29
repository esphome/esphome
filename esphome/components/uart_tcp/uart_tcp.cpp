#include "uart_tcp.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cerrno>
#include <cstdio>
#include <cstring>

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
#include "esphome/components/network/ip_address.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#else
#include <netdb.h>
#endif

namespace esphome::uart_tcp {

static const char *const TAG = "uart_tcp";

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
static void format_ipv4(uint32_t raw, char *dest, size_t dest_len) {
  ip_addr_t addr{};
  ip_addr_set_ip4_u32(&addr, raw);
  char buf[network::IP_ADDRESS_BUFFER_SIZE];
  network::IPAddress(&addr).str_to(buf);
  snprintf(dest, dest_len, "%s", buf);
}
#endif

static uint32_t loop_time() { return App.get_loop_component_start_time(); }

float UartTcp::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void UartTcp::setup() {
  this->last_attempt_ms_ = loop_time() - this->reconnect_interval_ms_;
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(false);
  }
}

void UartTcp::dump_config() {
  ESP_LOGCONFIG(TAG, "UART TCP:");
  ESP_LOGCONFIG(TAG, "  Role: %s", this->server_ ? "server" : "client");
  if (this->server_) {
    ESP_LOGCONFIG(TAG, "  Listen: %u", this->port_);
  } else {
    ESP_LOGCONFIG(TAG, "  Host: %s:%u", this->host_.c_str(), this->port_);
  }
  ESP_LOGCONFIG(TAG, "  UART baud: %" PRIu32, this->parent_->get_baud_rate());
  ESP_LOGCONFIG(TAG, "  Reconnect interval: %u ms", this->reconnect_interval_ms_);
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
}

void UartTcp::on_shutdown() {
  this->close_sock_();
  this->close_listen_();
}

void UartTcp::set_link_up_(bool up) {
  if (this->connected_ == up) {
    return;
  }
  this->connected_ = up;
  if (this->connected_sensor_ != nullptr) {
    this->connected_sensor_->publish_state(up);
  }
}

void UartTcp::close_sock_() {
  if (this->sock_ != nullptr) {
    this->sock_->shutdown(SHUT_RDWR);
    this->sock_->close();
    this->sock_.reset();
  }
  this->connecting_ = false;
  this->set_link_up_(false);
  this->forget_addr_();
}

void UartTcp::close_listen_() { this->listen_.reset(); }

void UartTcp::note_attempt_() { this->last_attempt_ms_ = loop_time(); }

void UartTcp::forget_addr_() {
  this->have_addr_.store(false);
  this->resolved_addr_.store(0);
  this->resolved_ip_[0] = '\0';
}

bool UartTcp::in_backoff_() const { return loop_time() - this->last_attempt_ms_ < this->reconnect_interval_ms_; }

void UartTcp::apply_socket_options_(socket::Socket *sock) {
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

#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
void UartTcp::dns_found(const char *name, const ip_addr_t *addr, void *arg) {
  auto *self = static_cast<UartTcp *>(arg);
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

void UartTcp::try_resolve_() {
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
#if !defined(USE_HOST) && !defined(USE_ZEPHYR)
  ip_addr_t cached;
  err_t err;
  {
    LwIPLock lock;
    err = dns_gethostbyname(this->host_.c_str(), &cached, &UartTcp::dns_found, this);
  }
  if (err == ERR_OK && IP_IS_V4(&cached)) {
    this->resolved_addr_.store(ip4_addr_get_u32(ip_2_ip4(&cached)));
    this->have_addr_.store(true);
    return;
  }
  if (err == ERR_INPROGRESS) {
    this->resolving_.store(true);
    return;
  }
#else
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

bool UartTcp::ip_ready_() {
  if (this->resolved_ip_[0] != '\0') {
    return true;
  }
  if (!this->have_addr_.load()) {
    return false;
  }
#if defined(USE_HOST) || defined(USE_ZEPHYR)
  return false;
#else
  format_ipv4(this->resolved_addr_.load(), this->resolved_ip_, sizeof(this->resolved_ip_));
  return this->resolved_ip_[0] != '\0';
#endif
}

void UartTcp::try_connect_() {
  if (this->sock_ != nullptr || this->in_backoff_()) {
    return;
  }
  if (this->resolve_failed_.exchange(false)) {
    this->have_addr_.store(false);
    this->resolved_ip_[0] = '\0';
    this->note_attempt_();
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
    this->note_attempt_();
    return;
  }
  this->sock_ = socket::socket(dest.ss_family, SOCK_STREAM, IPPROTO_TCP);
  if (this->sock_ == nullptr) {
    this->note_attempt_();
    return;
  }
  this->apply_socket_options_(this->sock_.get());
  int rc = this->sock_->connect(reinterpret_cast<struct sockaddr *>(&dest), dest_len);
  if (rc == 0 || errno == EINPROGRESS) {
    this->connecting_ = rc != 0;
    if (rc == 0) {
      this->set_link_up_(true);
      ESP_LOGI(TAG, "Connected to %s:%u", this->host_.c_str(), this->port_);
    }
    return;
  }
  this->sock_.reset();
  this->forget_addr_();
  this->note_attempt_();
}

void UartTcp::try_listen_() {
  if (this->listen_ != nullptr || this->in_backoff_()) {
    return;
  }
  this->listen_ = socket::socket_ip_loop_monitored(SOCK_STREAM, IPPROTO_TCP);
  if (this->listen_ == nullptr) {
    this->note_attempt_();
    return;
  }
  int yes = 1;
  this->listen_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  this->listen_->setblocking(false);
  struct sockaddr_storage local;
  socklen_t local_len =
      socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->port_);
  if (local_len == 0 || this->listen_->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) != 0 ||
      this->listen_->listen(1) != 0) {
    ESP_LOGW(TAG, "Listen on %u failed", this->port_);
    this->listen_.reset();
    this->note_attempt_();
    return;
  }
  ESP_LOGI(TAG, "Listening on %u", this->port_);
}

void UartTcp::accept_client_() {
  if (this->listen_ == nullptr || this->sock_ != nullptr) {
    return;
  }
  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof(peer);
  auto client = this->listen_->accept(reinterpret_cast<struct sockaddr *>(&peer), &peer_len);
  if (client == nullptr) {
    return;
  }
  this->apply_socket_options_(client.get());
  this->sock_ = std::move(client);
  this->set_link_up_(true);
  ESP_LOGI(TAG, "Client connected");
}

void UartTcp::read_socket_() {
  if (this->sock_ == nullptr) {
    return;
  }
  if (this->connecting_) {
    int err = 0;
    switch (socket::poll_connect(*this->sock_, err)) {
      case socket::ConnectPollResult::CONNECT_POLL_RESULT_PENDING:
        return;
      case socket::ConnectPollResult::CONNECT_POLL_RESULT_ERROR:
        ESP_LOGW(TAG, "Connection failed: %d", err);
        this->close_sock_();
        this->note_attempt_();
        return;
      default:
        break;
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
    this->note_attempt_();
    return;
  }
  if (count > 0) {
    this->write_array(tmp, static_cast<size_t>(count));
  }
}

void UartTcp::send_all_(const uint8_t *data, size_t len) {
  if (!this->connected_ || this->sock_ == nullptr || len == 0) {
    return;
  }
  size_t sent_total = 0;
  while (sent_total < len) {
    ssize_t sent = this->sock_->write(data + sent_total, len - sent_total);
    if (sent > 0) {
      sent_total += static_cast<size_t>(sent);
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    ESP_LOGW(TAG, "Send failed");
    this->close_sock_();
    this->note_attempt_();
    return;
  }
}

void UartTcp::read_uart_() {
  if (!this->connected_) {
    return;
  }
  uint8_t tmp[128];
  while (this->available() > 0) {
    size_t want = this->available();
    if (want > sizeof(tmp)) {
      want = sizeof(tmp);
    }
    if (!this->read_array(tmp, want)) {
      break;
    }
    this->send_all_(tmp, want);
    if (!this->connected_) {
      break;
    }
  }
}

void UartTcp::loop() {
  if (this->server_) {
    this->try_listen_();
    this->accept_client_();
  } else {
    this->try_connect_();
  }
  this->read_socket_();
  this->read_uart_();
}

}  // namespace esphome::uart_tcp

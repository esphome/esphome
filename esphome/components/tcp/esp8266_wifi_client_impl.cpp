#include "esphome/core/defines.h"
#ifdef USE_TCP_ESP8266_WIFI_CLIENT

#define LWIP_INTERNAL
extern "C" {
  #include "include/wl_definitions.h"
}

#include "esp8266_wifi_client_impl.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "lwip/tcp.h"

namespace esphome {
namespace tcp {

static const char *TAG = "tcp";

size_t ESP8266WiFiClientImpl::available() {
  return this->client_.available();
}
void ESP8266WiFiClientImpl::assert_read_(size_t size) {
  this->assert_not_closed_();
  size_t available = this->available();
  if (available < size) {
    ESP_LOGW(TAG, "Tried to read %u bytes, but only %u are known to be available!",  // NOLINT
             size, available);
    ESP_LOGV(TAG, "Developers: All calls to read() should check first if enough data is available. "
                  "Otherwise the system may block for a long time.");
  }
}
void ESP8266WiFiClientImpl::assert_not_closed_() {
  if (this->is_closed()) {
    ESP_LOGW(TAG, "Tried to operate on a closed socket!");
  }
}
bool ESP8266WiFiClientImpl::read(uint8_t *buffer, size_t size) {
  this->assert_read_(size);
  return this->client_.read(buffer, size) == size;
}
bool ESP8266WiFiClientImpl::write(const uint8_t *buffer, size_t size) {
  this->assert_not_closed_();
  this->drain_reserve_buffer_();

  size_t combined_avail_for_write = this->available_for_write();
  if (size > combined_avail_for_write) {
    ESP_LOGW(TAG, "write: Attempted to write more than allocated (%u of %u)!", size, combined_avail_for_write);
    // push_back will handle auto-resizing
  }

  size_t sendbuf_size = this->client_.availableForWrite();
  if (this->reserve_buffer_.empty()) {
    // reserve buffer is empty, write directly to TCP
    size_t to_write = std::min(sendbuf_size, size);
    this->write_internal_(buffer, to_write);
    if (to_write < size) {
      // write rest in reserve buffer
      this->reserve_buffer_.push_back(buffer + to_write, size - to_write);
    }
  } else {
    // reserve buffer has data, write to reserve buffer
    this->reserve_buffer_.push_back(buffer, size);
  }
}
void ESP8266WiFiClientImpl::drain_reserve_buffer_() {
  while (!this->reserve_buffer_.empty()) {
    size_t sendbuf_size = this->client_.availableForWrite();
    if (sendbuf_size == 0)
      return;
    auto pair = this->reserve_buffer_.pop_front_linear(sendbuf_size);
    this->write_internal_(pair.first, pair.second);
  }
}
void ESP8266WiFiClientImpl::write_internal_(const uint8_t *data, size_t len) { this->client_.write(data, len); }
bool ESP8266WiFiClientImpl::flush() {
  this->assert_not_closed_();
  this->client_.flush();
  // flush() does not return anything, so use is_writable as a proxy (not 100% accurate).
  return this->is_writable();
}
bool ESP8266WiFiClientImpl::connect(IPAddress ip, uint16_t port) {
  // returns 0 if unsuccessful, 1 on success
  int res = this->client_.connect(ip, port);
  this->initialized_ = false;
  this->host_ = ip.toString().c_str();
  return res != 0;
}
bool ESP8266WiFiClientImpl::connect(const std::string &host, uint16_t port) {
  int res = this->client_.connect(host.c_str(), port);
  this->initialized_ = false;
  this->host_ = host;
  return res != 0;
}
void ESP8266WiFiClientImpl::close(bool force) {
  this->initialized_ = true;
  this->client_.stop();
}
IPAddress ESP8266WiFiClientImpl::get_remote_address() { return this->client_.remoteIP(); }
uint16_t ESP8266WiFiClientImpl::get_remote_port() { return this->client_.remotePort(); }
IPAddress ESP8266WiFiClientImpl::get_local_address() { return this->client_.localIP(); }
uint16_t ESP8266WiFiClientImpl::get_local_port() { return this->client_.localPort(); }
void ESP8266WiFiClientImpl::set_no_delay(bool no_delay) {
  this->assert_not_closed_();
  this->client_.setNoDelay(no_delay);
}
void ESP8266WiFiClientImpl::set_timeout(uint32_t timeout_ms) {
  this->assert_not_closed_();
  this->client_.setTimeout(timeout_ms);
}
ESP8266WiFiClientImpl::~ESP8266WiFiClientImpl() { this->client_.stop(); }
TCPSocket::State ESP8266WiFiClientImpl::state() {
  if (this->initialized_) {
    return TCPSocket::STATE_INITIALIZED;
  }
  int status = this->client_.status();
  switch (status) {
    case CLOSED:
      return TCPSocket::STATE_CLOSED;
    case LISTEN:
    case SYN_SENT:
    case SYN_RCVD:
      return TCPSocket::STATE_CONNECTING;
    case ESTABLISHED:
      return TCPSocket::STATE_CONNECTED;
    case FIN_WAIT_1:
    case FIN_WAIT_2:
    case CLOSE_WAIT:
    case CLOSING:
    case LAST_ACK:
    case TIME_WAIT:
      return TCPSocket::STATE_CLOSING;
    default:
      ESP_LOGV(TAG, "Unknown state %d for client!", status);
      return TCPSocket::STATE_CLOSED;
  }
}
size_t ESP8266WiFiClientImpl::available_for_write() {
  return this->client_.availableForWrite() + this->reserve_buffer_.capacity();
}
void ESP8266WiFiClientImpl::reserve_at_least(size_t size) {
  if (size <= TCP_SND_BUF)
    return;
  this->reserve_buffer_.reserve(size - TCP_SND_BUF);
}
void ESP8266WiFiClientImpl::ensure_capacity(size_t size) {
  size_t avail = this->available_for_write();
  if (size <= avail)
    return;
  this->reserve_buffer_.reserve(size - avail);
}
void ESP8266WiFiClientImpl::loop() {
  if (this->is_writable())
    this->drain_reserve_buffer_();
}
std::string ESP8266WiFiClientImpl::get_host() { return this->host_; }
ESP8266WiFiClientImpl::ESP8266WiFiClientImpl(const WiFiClient &client) : TCPClient() {
  this->client_ = client;
  this->host_ = this->client_.remoteIP().toString().c_str();
  this->initialized_ = false;
}

// ========================== WiFi Server ==========================
bool ESP8266WiFiServerImpl::bind(uint16_t port) {
  this->server_ = make_unique<WiFiServer>(port);
  this->server_->begin(port);
  return true;
}
std::unique_ptr<TCPSocket> ESP8266WiFiServerImpl::accept() {
  auto client = this->server_->available();
  if (client.connected()) {
    return std::unique_ptr<TCPSocket>(new ESP8266WiFiClientImpl(client));
  }
  return std::unique_ptr<TCPSocket>();
}
void ESP8266WiFiServerImpl::close(bool force) { this->server_->close(); }

}  // namespace tcp
}  // namespace esphome

#endif  // USE_TCP_ESP8266_WIFI_CLIENT

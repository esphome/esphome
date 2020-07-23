#pragma once

#include "esphome/core/defines.h"
#ifdef USE_TCP_ESP8266_WIFI_CLIENT

#include "tcp.h"
#include "ring_buffer.h"
#include "WiFiClient.h"
#include "WiFiServer.h"

namespace esphome {
namespace tcp {

class ESP8266WiFiClientImpl : public TCPClient {
 public:
  ESP8266WiFiClientImpl() = default;
  ~ESP8266WiFiClientImpl() override;
  size_t available() override;
  bool read(uint8_t *buffer, size_t size) override;
  bool write(const uint8_t *buffer, size_t size) override;
  bool flush() override;
  bool connect(IPAddress ip, uint16_t port) override;
  bool connect(const std::string &host, uint16_t port) override;
  void close(bool force) override;
  IPAddress get_remote_address() override;
  uint16_t get_remote_port() override;
  IPAddress get_local_address() override;
  uint16_t get_local_port() override;
  void set_no_delay(bool no_delay) override;
  void set_timeout(uint32_t timeout_ms) override;
  State state() override;
  void loop() override;
  std::string get_host() override;
  size_t available_for_write() override;
  void reserve_at_least(size_t size) override;
  void ensure_capacity(size_t size) override;

 protected:
  void assert_not_closed_();
  void assert_read_(size_t size);
  void drain_reserve_buffer_();
  void write_internal_(const uint8_t *data, size_t len);

  friend class ESP8266WiFiServerImpl;
  /// Construct a socket from a client object
  explicit ESP8266WiFiClientImpl(const WiFiClient &client);

  bool initialized_{true};
  WiFiClient client_;
  RingBuffer reserve_buffer_;
  std::string host_;
};

class ESP8266WiFiServerImpl : public TCPServer {
 public:
  bool bind(uint16_t port) override;
  std::unique_ptr<TCPSocket> accept() override;
  void close(bool force) override;

 protected:
  std::unique_ptr<WiFiServer> server_;
};

}  // namespace tcp
}  // namespace esphome

#endif  // USE_TCP_ESP8266_WIFI_CLIENT

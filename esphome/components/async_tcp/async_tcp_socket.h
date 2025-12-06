#pragma once

#ifndef USE_ARDUINO

#include "esphome/components/socket/socket.h"
#include <functional>
#include <memory>
#include <string>

// Provide AsyncClient API for ESP-IDF and host platforms using sockets
class AsyncClient {
 public:
  using AcConnectHandler = std::function<void(void *, AsyncClient *)>;
  using AcDataHandler = std::function<void(void *, AsyncClient *, void *data, size_t len)>;
  using AcErrorHandler = std::function<void(void *, AsyncClient *, int8_t error)>;

  AsyncClient() = default;
  ~AsyncClient() = default;

  bool connect(const char *host, uint16_t port);
  void close();
  bool connected() const { return connected_; }
  size_t write(const char *data, size_t len);

  void onConnect(AcConnectHandler cb, void *arg = nullptr) {
    connect_cb_ = cb;
    connect_arg_ = arg;
  }
  void onDisconnect(AcConnectHandler cb, void *arg = nullptr) {
    disconnect_cb_ = cb;
    disconnect_arg_ = arg;
  }
  void onData(AcDataHandler cb, void *arg = nullptr) {
    data_cb_ = cb;
    data_arg_ = arg;
  }
  void onError(AcErrorHandler cb, void *arg = nullptr) {
    error_cb_ = cb;
    error_arg_ = arg;
  }

  // Must be called from loop()
  void loop();

 private:
  std::unique_ptr<esphome::socket::Socket> socket_;
  bool connected_{false};
  bool connecting_{false};

  AcConnectHandler connect_cb_{nullptr};
  void *connect_arg_{nullptr};
  AcConnectHandler disconnect_cb_{nullptr};
  void *disconnect_arg_{nullptr};
  AcDataHandler data_cb_{nullptr};
  void *data_arg_{nullptr};
  AcErrorHandler error_cb_{nullptr};
  void *error_arg_{nullptr};
};

#endif  // !USE_ARDUINO

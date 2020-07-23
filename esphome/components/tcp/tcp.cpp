#include "tcp.h"
#include "esp8266_wifi_client_impl.h"
#include "lwip_raw_tcp_impl.h"
#include "unix_socket_impl.h"
#include "async_tcp_impl.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tcp {

static const char *TAG = "tcp";

std::unique_ptr<TCPSocket> make_socket() {
#ifdef USE_TCP_ESP8266_WIFI_CLIENT
  return make_unique<ESP8266WiFiClientImpl>();
#elif defined(USE_TCP_LWIP_RAW_TCP)
  return make_unique<LWIPRawTCPImpl>();
#elif defined(USE_TCP_ESP32_WIFI_CLIENT)
  // TODO
#elif defined(USE_TCP_ASYNC_TCP)
  return make_unique<AsyncTCPImpl>();
#elif defined(USE_TCP_UNIX_SOCKET)
  return make_unique<UnixSocketImpl>();
#else
#error No make_socket backend selected
#endif
}

std::unique_ptr<TCPServer> make_server() {
#ifdef USE_TCP_ESP8266_WIFI_CLIENT
  return make_unique<ESP8266WiFiServerImpl>();
#elif defined(USE_TCP_LWIP_RAW_TCP)
  return make_unique<LWIPRawTCPServerImpl>();
#elif defined(USE_TCP_ESP32_WIFI_CLIENT)
  // TODO
#elif defined(USE_TCP_ASYNC_TCP)
  return make_unique<AsyncTCPServerImpl>();
#elif defined(USE_TCP_UNIX_SOCKET)
  return make_unique<UnixServerImpl>();
#else
#error No make_server backend selected
#endif
}

const char *socket_state_to_string(TCPSocket::State state) {
  switch (state) {
    case TCPSocket::STATE_INITIALIZED:
      return "INITIALIZED";
    case TCPSocket::STATE_CONNECTING:
      return "CONNECTING";
    case TCPSocket::STATE_CONNECTED:
      return "CONNECTED";
    case TCPSocket::STATE_CLOSING:
      return "CLOSING";
    case TCPSocket::STATE_CLOSED:
      return "CLOSED";
    default:
      return "UNKOWN";
  }
}

}  // namespace tcp
}  // namespace esphome

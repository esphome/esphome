#include "tcp.h"
#include "esp8266_wifi_client_impl.h"
#include "lwip_raw_tcp_impl.h"
#include "unix_socket_impl.h"
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
#elif defined(USE_TCP_UNIX_SOCKET)
  return make_unique<UnixSocketImpl>();
#endif
}

std::unique_ptr<TCPServer> make_server() {
#ifdef USE_TCP_ESP8266_WIFI_CLIENT
  return make_unique<ESP8266WiFiServerImpl>();
#elif defined(USE_TCP_LWIP_RAW_TCP)
  return make_unique<LWIPRawTCPServerImpl>();
#elif defined(USE_TCP_ESP32_WIFI_CLIENT)
#elif defined(USE_TCP_UNIX_SOCKET)
  return make_unique<UnixServerImpl>();
#endif
}

}  // namespace tcp
}  // namespace esphome

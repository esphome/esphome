// Stub for benchmark builds — provides the minimal interface that
// api_connection.cpp needs when USE_ZWAVE_PROXY is defined,
// without pulling in the real UART-based ZWaveProxy implementation.
#pragma once

#include "esphome/components/api/api_pb2.h"

namespace esphome {
namespace api {
class APIConnection;
}  // namespace api

namespace zwave_proxy {

class ZWaveProxy {
 public:
  api::APIConnection *get_api_connection() { return nullptr; }
  void zwave_proxy_request(api::APIConnection *conn, api::enums::ZWaveProxyRequestType type) {}
  void send_frame(const uint8_t *data, size_t length) {}
  void api_connection_authenticated(api::APIConnection *conn) {}
  uint32_t get_feature_flags() const { return 0; }
  uint32_t get_home_id() { return 0; }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ZWaveProxy *global_zwave_proxy;

}  // namespace zwave_proxy
}  // namespace esphome

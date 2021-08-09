#pragma once
#include <memory>
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace ssl {

class SSLContext {
 public:
  SSLContext() = default;
  virtual ~SSLContext() = default;
  SSLContext(const SSLContext&) = delete;
  SSLContext &operator=(const SSLContext &) = delete;

  virtual int init() = 0;
  virtual void set_server_certificate(const char *cert) = 0;
  virtual void set_private_key(const char *private_key) = 0;
  virtual std::unique_ptr<socket::Socket> wrap_socket(std::unique_ptr<socket::Socket> sock) = 0;
};

std::unique_ptr<SSLContext> create_context();

}  // namespace ssl
}  // namespace esphome

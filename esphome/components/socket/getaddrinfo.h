#pragma once
#include <memory>
#include "headers.h"

namespace esphome {
namespace socket {

struct GetaddrinfoFuture {
 public:
  virtual ~GetaddrinfoFuture() = default;
  // returns true when the request has completed (successfully or with an error)
  virtual bool completed() = 0;
  /**
   * @brief Fetch the completed result into res.
   *
   * Should only be called after completed() returned true.
   * Make sure to call freeaddrinfo() to free the addrinfo storage
   * when it's no longer needed.
   *
   * @return See posix getaddrinfo() return values.
   */
  virtual int fetch_result(struct addrinfo **res) = 0;
};

std::unique_ptr<GetaddrinfoFuture> getaddrinfo_async(const char *node, const char *service,
                                                     const struct addrinfo *hints);

}  // namespace socket
}  // namespace esphome

#ifdef USE_ESP8266
void freeaddrinfo(struct addrinfo *ai);
const char *gai_strerror(int errcode);
#endif

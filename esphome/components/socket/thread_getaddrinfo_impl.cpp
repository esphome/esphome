#include "getaddrinfo.h"
#include "esphome/core/defines.h"

#ifndef USE_SOCKET_HAS_LWIP

#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace socket {

static const char *const TAG = "socket.threadgetaddrinfo";

struct ThreadGetaddrinfoResult {
  bool completed;
  int return_code;
  struct addrinfo *res;
};

class ThreadGetaddrinfoFuture : public GetaddrinfoFuture {
 public:
  ThreadGetaddrinfoFuture(std::shared_ptr<ThreadGetaddrinfoResult> result) : result_(result) {}
  ~ThreadGetaddrinfoFuture() override = default;

  bool completed() override { return result_->completed; }
  int fetch_result(struct addrinfo **res) {
    if (res == nullptr)
      return EAI_FAIL;
    *res = nullptr;
    if (!result_->completed)
      return EAI_FAIL;
    if (result_->return_code != 0)
      return result_->return_code;

    *res = result_->res;
    return 0;
  }

 protected:
  std::shared_ptr<ThreadGetaddrinfoResult> result_;
};

void worker(std::shared_ptr<ThreadGetaddrinfoResult> result, const char *node, const char *service,
            const struct addrinfo *hints) {
  result->return_code = getaddrinfo(node, service, hints, &result->res);
  result->completed = true;
  if (hints != nullptr) {
    delete hints->ai_addr;
    delete hints->ai_canonname;
    delete hints;
  }
  delete node;
  delete service;
}

std::unique_ptr<GetaddrinfoFuture> getaddrinfo_async(const char *node, const char *service,
                                                     const struct addrinfo *hints) {
  std::shared_ptr<ThreadGetaddrinfoResult> result = std::make_shared<ThreadGetaddrinfoResult>();
  result->completed = false;

  struct addrinfo *hints_copy = nullptr;
  if (hints != nullptr) {
    hints_copy = new struct addrinfo;
    hints_copy->ai_flags = hints->ai_flags;
    hints_copy->ai_family = hints->ai_family;
    hints_copy->ai_socktype = hints->ai_socktype;
    hints_copy->ai_protocol = hints->ai_protocol;
    hints_copy->ai_addrlen = hints->ai_addrlen;
    if (ai->ai_addr != nullptr) {
      hints_copy->ai_addr = malloc(hints->ai_addrlen);
      memcpy(hints_copy->ai_addr, hints->ai_addr, hints->ai_addrlen);
    }
    if (ai->ai_canonname != nullptr) {
      hints_copy->ai_canonname = strdup(hints->ai_canonname);
    }
    hints_copy->ai_next = nullptr;
  }

  const char *node_copy = nullptr, *service_copy = nullptr;
  if (node != nullptr)
    node_copy = strdup(node);
  if (service != nullptr)
    service_copy = strdup(service);

  std::thread thread(worker, result, node_copy, service_copy, hints_copy);
  thread.detach();

  return std::unique_ptr<GetaddrinfoFuture>{new ThreadGetaddrinfoFuture(result)};
}

}  // namespace socket
}  // namespace esphome

#endif  // !USE_SOCKET_HAS_LWIP

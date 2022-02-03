#include "getaddrinfo.h"
#include "esphome/core/defines.h"

#ifdef USE_SOCKET_HAS_LWIP

#include <utility>
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/netdb.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace socket {

static const char *const TAG = "socket.lwipgetaddrinfo";

struct LwipDNSResult {
  bool completed;
  bool error;
  ip_addr_t ipaddr;
};

struct LwipDNSCallbackArg {
  std::weak_ptr<LwipDNSResult> res;
};

void lwip_dns_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  LwipDNSCallbackArg *arg = reinterpret_cast<LwipDNSCallbackArg *>(callback_arg);
  {
    std::shared_ptr<LwipDNSResult> result = arg->res.lock();
    if (result) {
      if (ipaddr == nullptr) {
        result->error = true;
      } else {
        result->error = false;
        ip_addr_copy(result->ipaddr, *ipaddr);
      }
      result->completed = true;
    }
  }
  delete arg;  // NOLINT(cppcoreguidelines-owning-memory)
}

class LwipGetaddrinfoFuture : public GetaddrinfoFuture {
 public:
  LwipGetaddrinfoFuture(std::shared_ptr<LwipDNSResult> result, int hint_ai_socktype, int hint_ai_protocol,
                        uint16_t portno)
      : result_(std::move(result)),
        hint_ai_socktype_(hint_ai_socktype),
        hint_ai_protocol_(hint_ai_protocol),
        portno_(portno) {}
  ~LwipGetaddrinfoFuture() override = default;

  bool completed() override { return result_->completed; }
  int fetch_result(struct addrinfo **res) override {
    if (res == nullptr)
      return EAI_FAIL;
    *res = nullptr;
    if (!result_->completed)
      return EAI_FAIL;
    if (result_->error)
      return EAI_FAIL;

    size_t alloc_size = sizeof(struct addrinfo) + sizeof(struct sockaddr_storage);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
    void *storage = malloc(alloc_size);
    memset(storage, 0, alloc_size);
    struct addrinfo *ai = reinterpret_cast<struct addrinfo *>(storage);
    struct sockaddr_storage *sa = reinterpret_cast<struct sockaddr_storage *>(ai + 1);

    bool isipv6 = IP_IS_V6(result_->ipaddr);

    bool istcp = true;
    if ((hint_ai_socktype_ != 0 && hint_ai_socktype_ == SOCK_DGRAM) ||
        (hint_ai_protocol_ != 0 && hint_ai_protocol_ == IPPROTO_UDP)) {
      istcp = false;
    }

    ai->ai_family = isipv6 ? AF_INET6 : AF_INET;
    ai->ai_socktype = istcp ? SOCK_STREAM : SOCK_DGRAM;
    ai->ai_protocol = istcp ? IPPROTO_TCP : IPPROTO_UDP;

    if (isipv6) {
#if LWIP_IPV6
      struct sockaddr_in6 *sa6 = reinterpret_cast<struct sockaddr_in6 *>(sa);
      inet6_addr_from_ip6addr(&sa6->sin6_addr, ip_2_ip6(&result_->ipaddr)) sa6->sin6_family = AF_INET6;
      sa6->sin6_len = sizeof(struct sockaddr_in6);
      sa6->sin6_port = htons(portno_);
#endif  // LWIP_IPV6
    } else {
      struct sockaddr_in *sa4 = reinterpret_cast<struct sockaddr_in *>(sa);
      inet_addr_from_ip4addr(&sa4->sin_addr, ip_2_ip4(&result_->ipaddr));
      sa4->sin_family = AF_INET;
      sa4->sin_len = sizeof(struct sockaddr_in);
      sa4->sin_port = htons(portno_);
    }

    ai->ai_addrlen = sizeof(struct sockaddr_storage);
    ai->ai_addr = reinterpret_cast<struct sockaddr *>(sa);
    *res = ai;
    return 0;
  }

 protected:
  std::shared_ptr<LwipDNSResult> result_;
  int hint_ai_socktype_;
  int hint_ai_protocol_;
  uint16_t portno_;
};

std::unique_ptr<GetaddrinfoFuture> getaddrinfo_async(const char *node, const char *service,
                                                     const struct addrinfo *hints) {
  std::shared_ptr<LwipDNSResult> result = std::make_shared<LwipDNSResult>();
  result->completed = false;

  uint16_t portno = 0;
  if (service != nullptr) {
    optional<uint16_t> i = parse_number<uint16_t>(service);
    if (!i.has_value()) {
      result->completed = true;
      result->error = true;
      return std::unique_ptr<GetaddrinfoFuture>{new LwipGetaddrinfoFuture(result, 0, 0, 0)};
    }
    portno = *i;
  }

  int hint_ai_socktype = 0, hint_ai_protocol = 0;
  uint8_t dns_addrtype = LWIP_DNS_ADDRTYPE_DEFAULT;
  if (hints != nullptr) {
    hint_ai_socktype = hints->ai_socktype;
    hint_ai_protocol = hints->ai_protocol;
    if (hints->ai_family == AF_INET) {
      dns_addrtype = LWIP_DNS_ADDRTYPE_IPV4;
    } else if (hints->ai_family == AF_INET6) {
      dns_addrtype = LWIP_DNS_ADDRTYPE_IPV6;
    }
  }

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
  LwipDNSCallbackArg *callback_arg = new LwipDNSCallbackArg;
  callback_arg->res = result;

  ip_addr_t immediate_result;
  err_t err = dns_gethostbyname_addrtype(node, &immediate_result, lwip_dns_callback, callback_arg, dns_addrtype);
  if (err == ERR_OK) {
    // immediate result
    result->completed = true;
    result->error = false;
    ip_addr_copy(result->ipaddr, immediate_result);

    // callback won't be called
    delete callback_arg;  // NOLINT(cppcoreguidelines-owning-memory)
  } else if (err == ERR_INPROGRESS) {
    // result notified via callback
  } else {
    // error
    result->completed = true;
    result->error = true;

    // callback won't be called
    delete callback_arg;  // NOLINT(cppcoreguidelines-owning-memory)
  }

  return std::unique_ptr<GetaddrinfoFuture>{
      new LwipGetaddrinfoFuture(result, hint_ai_socktype, hint_ai_protocol, portno)};
}

}  // namespace socket
}  // namespace esphome

#ifdef USE_ESP8266
void freeaddrinfo(struct addrinfo *ai) {
  while (ai != nullptr) {
    struct addrinfo *next = ai->ai_next;
    delete ai;  // NOLINT(cppcoreguidelines-owning-memory)
    ai = next;
  }
}
const char *gai_strerror(int errcode) {
  switch (errcode) {
    case EAI_BADFLAGS: return "badflags";
    case EAI_NONAME: return "noname";
    case EAI_AGAIN: return "again";
    case EAI_FAMILY: return "family";
    case EAI_SOCKTYPE: return "socktype";
    case EAI_SERVICE: return "service";
    case EAI_MEMORY: return "memory";
    case EAI_SYSTEM: return "system";
    case EAI_OVERFLOW: return "overflow";
    default: return "unknown";
  }
}
#endif

#endif  // USE_SOCKET_HAS_LWIP

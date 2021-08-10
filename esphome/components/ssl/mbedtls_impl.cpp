#include "ssl_context.h"
#include <string.h>
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#ifdef INADDR_NONE
#undef INADDR_NONE
#endif
#include "esphome/core/log.h"

namespace esphome {
namespace ssl {

static const char *const TAG = "ssl.mbedtls";

static int entropy_hw_random_source(void *data, uint8_t *output, size_t len, size_t *olen) {
  esp_fill_random(output, len);
	*olen = len;
  return 0;
}

struct MbedTLSBioCtx {
  socket::Socket *sock;

  static int send(void *raw, const uint8_t *buf, size_t len) {
    auto *ctx = reinterpret_cast<MbedTLSBioCtx *>(raw);
    ssize_t ret = ctx->sock->write(buf, len);
    if (ret != -1)
      return ret;
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    if (errno == EPIPE || errno == ECONNRESET)
      return MBEDTLS_ERR_NET_CONN_RESET;
    if (errno == EINTR)
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    return MBEDTLS_ERR_NET_SEND_FAILED;
  }
  static int recv(void *raw, uint8_t *buf, size_t len) {
    auto *ctx = reinterpret_cast<MbedTLSBioCtx *>(raw);
    ssize_t ret = ctx->sock->read(buf, len);
    if (ret != -1)
      return ret;
    if (errno == EWOULDBLOCK || errno == EAGAIN)
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    if (errno == EPIPE || errno == ECONNRESET)
      return MBEDTLS_ERR_NET_CONN_RESET;
    if (errno == EINTR)
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    return MBEDTLS_ERR_NET_SEND_FAILED;
  }
};

class MbedTLSWrappedSocket : public socket::Socket {
 public:
  MbedTLSWrappedSocket(std::unique_ptr<socket::Socket> sock)
  : socket::Socket(), sock_(std::move(sock)) {}
  ~MbedTLSWrappedSocket() override {
    mbedtls_ssl_free(&ssl_);
    sock_ = nullptr;
  }
  void init(const mbedtls_ssl_config *conf) {
    // TODO: reuse ssl contexts?
    mbedtls_ssl_init(&ssl_);
    int err = mbedtls_ssl_setup(&ssl_, conf);
    if (err != 0) {
      ESP_LOGW(TAG, "mbedtls_ssl_setup failed: %d", err);
      return;
    }
    // sock pointer does not fit in void*
    // instead store it in a heap-allocated var
    auto *ctx = new MbedTLSBioCtx;
    // unsafe, but should be fine because we free before sock is reset
    ctx->sock = sock_.get();
    mbedtls_ssl_set_bio(&ssl_, ctx, MbedTLSBioCtx::send, MbedTLSBioCtx::recv, nullptr);

    do_handshake_ = true;
  }

  std::unique_ptr<Socket> accept(struct sockaddr *addr, socklen_t *addrlen) override {
    // only for server sockets
    errno = EBADF;
    return {};
  }
  int bind(const struct sockaddr *addr, socklen_t addrlen) override {
    errno = EBADF;
    return -1;
  }
  int close() override {
    return sock_->close();
  }
  int connect(const std::string &address) override {
    return sock_->connect(address);
  }
  int connect(const struct sockaddr *addr, socklen_t addrlen) override {
    return sock_->connect(addr, addrlen);
  }
  int shutdown(int how) override {
    int ret = mbedtls_ssl_close_notify(&ssl_);
    if (ret != 0)
      return this->mbedtls_to_errno_(ret);
    return this->sock_->shutdown(how);
  }

  int getpeername(struct sockaddr *addr, socklen_t *addrlen) override {
    return sock_->getpeername(addr, addrlen);
  }
  std::string getpeername() override {
    return sock_->getpeername();
  }
  int getsockname(struct sockaddr *addr, socklen_t *addrlen) override {
    return sock_->getsockname(addr, addrlen);
  }
  std::string getsockname() override {
    return sock_->getsockname();
  }
  int getsockopt(int level, int optname, void *optval, socklen_t *optlen) override {
    return sock_->getsockopt(level, optname, optval, optlen);
  }
  int setsockopt(int level, int optname, const void *optval, socklen_t optlen) override {
    return sock_->setsockopt(level, optname, optval, optlen);
  }
  int listen(int backlog) override {
    errno = EBADF;
    return -1;
  }
  ssize_t read(void *buf, size_t len) override {
    // mbedtls will automatically perform handshake here if necessary
    int ret = mbedtls_ssl_read(&ssl_, reinterpret_cast<uint8_t *>(buf), len);
    return this->mbedtls_to_errno_(ret);
  }
  // virtual ssize_t readv(const struct iovec *iov, int iovcnt) = 0;
  ssize_t write(const void *buf, size_t len) override {
    int ret = mbedtls_ssl_write(&ssl_, reinterpret_cast<const uint8_t *>(buf), len);
    return this->mbedtls_to_errno_(ret);
  }
  // virtual ssize_t writev(const struct iovec *iov, int iovcnt) = 0;
  int setblocking(bool blocking) override {
    // TODO: handle blocking modes
    return sock_->setblocking(blocking);
  }

 protected:
  int mbedtls_to_errno_(int ret) {
    if (ret >= 0) {
      return ret;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
      errno = EWOULDBLOCK;
      return -1;
    } else if (ret == MBEDTLS_ERR_NET_CONN_RESET) {
      errno = ECONNRESET;
      return -1;
    } else {
      if (errno == 0)
        errno = EIO;
      ESP_LOGW(TAG, "mbedtls failed with %d", ret);
      return -1;
    }
  }

  std::unique_ptr<socket::Socket> sock_;
  mbedtls_ssl_context ssl_;
  bool do_handshake_ = false;
};

void debug_callback(void *arg, int level, const char *filename, int line_number, const char *msg) {
  ESP_LOGV(TAG, "mbedtls %d [%s:%d]: %s", level, filename, line_number, msg);
}

class MbedTLSContext : public SSLContext {
 public:
  MbedTLSContext() = default;
  ~MbedTLSContext() override {
    mbedtls_pk_free(&pkey_);
    mbedtls_entropy_free(&entropy_);
    mbedtls_ctr_drbg_free(&ctr_drbg_);
    mbedtls_x509_crt_free(&srv_cert_);
    mbedtls_ssl_config_free(&conf_);
  }

  void set_server_certificate(const char *cert) override {
    this->srv_cert_str_ = cert;
  }
  void set_private_key(const char *private_key) override {
    this->privkey_str_ = private_key;
  }

  int init() override {
    mbedtls_x509_crt_init(&srv_cert_);
    mbedtls_ctr_drbg_init(&ctr_drbg_);
    mbedtls_entropy_init(&entropy_);
    mbedtls_pk_init(&pkey_);
    mbedtls_ssl_config_init(&conf_);

    // TODO check what this does
    int err = mbedtls_entropy_add_source(&entropy_, entropy_hw_random_source, NULL, 134, MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (err != 0) {
      ESP_LOGW(TAG, "mbedtls_entropy_add_source failed: %d", err);
      return 1;
    }
    err = mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_, NULL, 0);
    if (err != 0) {
      ESP_LOGW(TAG, "mbedtls_ctr_drbg_seed failed: %d", err);
      return 1;
    }

    err = mbedtls_x509_crt_parse(
      &srv_cert_,
      reinterpret_cast<const uint8_t *>(srv_cert_str_),
      // "including the terminating NULL byte"
      strlen(srv_cert_str_) + 1
    );
    if (err != 0) {
      ESP_LOGW(TAG, "mbedtls_x509_crt_parse failed: %d", err);
      return 1;
    }

    err = mbedtls_pk_parse_key(
      &pkey_,
      reinterpret_cast<const uint8_t *>(privkey_str_),
      // "including the terminating NULL byte"
      strlen(privkey_str_) + 1,
      nullptr,
      0
    );
    if (err != 0) {
      ESP_LOGW(TAG, "mbedtls_pk_parse_key failed: %d", err);
      return 1;
    }

    err = mbedtls_ssl_config_defaults(
      &conf_,
      MBEDTLS_SSL_IS_SERVER,
      MBEDTLS_SSL_TRANSPORT_STREAM,
      MBEDTLS_SSL_PRESET_DEFAULT
    );
    if (err != 0) {
      ESP_LOGW(TAG, "mbedtls_ssl_config_defaults failed: %d", err);
      return 1;
    }
    mbedtls_ssl_conf_rng(&conf_, mbedtls_ctr_drbg_random, &ctr_drbg_);
    err = mbedtls_ssl_conf_own_cert(&conf_, &srv_cert_, &pkey_);
    if (err != 0) {
      ESP_LOGW(TAG, "mbedtls_ssl_conf_own_cert failed: %d", err);
      return 1;
    }
    mbedtls_ssl_conf_dbg(&conf_, debug_callback, nullptr);
    return 0;
  }

  std::unique_ptr<socket::Socket> wrap_socket(std::unique_ptr<socket::Socket> sock) override {
    auto *wrapped = new MbedTLSWrappedSocket(std::move(sock));
    wrapped->init(&conf_);
    return std::unique_ptr<socket::Socket>{wrapped};
  }

 protected:
  const char *srv_cert_str_ = nullptr;
  const char *privkey_str_ = nullptr;
  mbedtls_entropy_context entropy_;
  mbedtls_ctr_drbg_context ctr_drbg_;
  mbedtls_x509_crt srv_cert_;
  mbedtls_pk_context pkey_;
  mbedtls_ssl_config conf_;
};

std::unique_ptr<SSLContext> create_context() {
  return std::unique_ptr<SSLContext>{new MbedTLSContext()};
}

}  // namespace ssl
}  // namespace esphome

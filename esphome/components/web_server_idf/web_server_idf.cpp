#ifdef USE_ESP32

#include <algorithm>
#include <cstdarg>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <mbedtls/base64.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "utils.h"
#include "web_server_idf.h"

#ifdef USE_WEBSERVER_AUTH_DIGEST
#include <esp_random.h>
#include <esp_rom_md5.h>
#endif

#ifdef USE_WEBSERVER_OTA
#include <multipart_parser.h>
#include "multipart.h"  // For parse_multipart_boundary and other utils
#endif

// Include socket headers after Arduino headers to avoid IPADDR_NONE/INADDR_NONE macro conflicts
#include <cerrno>
#include <sys/socket.h>

namespace esphome::web_server_idf {

using web_server_base::advance_iovecs;
using web_server_base::write_stream_head;

// Status strings not provided by esp_http_server.h
#ifndef HTTPD_401
#define HTTPD_401 "401 Unauthorized"
#endif
#ifndef HTTPD_403
#define HTTPD_403 "403 Forbidden"
#endif
#ifndef HTTPD_409
#define HTTPD_409 "409 Conflict"
#endif
#ifndef HTTPD_422
#define HTTPD_422 "422 Unprocessable Entity"
#endif

ESPHOME_LOG_TAG(TAG, "web_server_idf");

// Every frame in this file is capped below one JSON arena.
// Measured at -Os on GCC 14; newer toolchains stay checked on purpose, older ones skip it.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14 && defined(__OPTIMIZE_SIZE__)
#pragma GCC diagnostic error "-Wstack-usage=2048"
#endif

// Chunk size for streaming request bodies; matches the Arduino AsyncWebServer buffer size.
// Buffers of this size must live on the heap - the httpd task stack is too small.
static constexpr size_t RECV_CHUNK_SIZE = 1460;
static constexpr size_t YIELD_INTERVAL_BYTES = 16 * 1024;  // Yield every 16KB to prevent watchdog

// Global instance to avoid guard variable (saves 8 bytes)
// This is initialized at program startup before any threads
namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
DefaultHeaders default_headers_instance;
}  // namespace

DefaultHeaders &DefaultHeaders::Instance() { return default_headers_instance; }

void DefaultHeaders::apply_(httpd_req_t *req, uint8_t skip_mask) const {
  for (size_t i = 0; i < this->headers_.size(); i++) {
    if (!(skip_mask & (1U << i)))
      httpd_resp_set_hdr(req, this->headers_[i].name, this->headers_[i].value);
  }
}

namespace {
// Non-blocking send function to prevent watchdog timeouts when TCP buffers are full
/**
 * Sends data on a socket in non-blocking mode.
 *
 * @param hd      HTTP server handle (unused).
 * @param sockfd  Socket file descriptor.
 * @param buf     Buffer to send.
 * @param buf_len Length of buffer.
 * @param flags   Flags for send().
 * @return
 *   - Number of bytes sent on success.
 *   - HTTPD_SOCK_ERR_INVALID if buf is nullptr.
 *   - HTTPD_SOCK_ERR_TIMEOUT if the send buffer is full (EAGAIN/EWOULDBLOCK).
 *   - HTTPD_SOCK_ERR_FAIL for other errors.
 */
[[maybe_unused]] int nonblocking_send(httpd_handle_t hd, int sockfd, const char *buf, size_t buf_len, int flags) {
  if (buf == nullptr) {
    return HTTPD_SOCK_ERR_INVALID;
  }

  // Use MSG_DONTWAIT to prevent blocking when TCP send buffer is full
  int ret = send(sockfd, buf, buf_len, flags | MSG_DONTWAIT);
  if (ret < 0) {
    const int err = errno;
    if (err == EAGAIN || err == EWOULDBLOCK) {
      // Buffer full - retry later
      return HTTPD_SOCK_ERR_TIMEOUT;
    }
    // Real error
    ESP_LOGD(TAG, "send error: errno %d", err);
    return HTTPD_SOCK_ERR_FAIL;
  }
  return ret;
}
}  // namespace

void AsyncWebServer::safe_close_with_shutdown(httpd_handle_t hd, int sockfd) {
  // CRITICAL: Shut down receive BEFORE closing to prevent lwIP race conditions
  //
  // The race condition occurs because close() initiates lwIP teardown while
  // the TCP/IP thread can still receive packets, causing assertions when
  // recv_tcp() sees partially-torn-down state.
  //
  // By shutting down receive first, we tell lwIP to stop accepting new data BEFORE
  // the teardown begins, eliminating the race window. We only shutdown RD (not RDWR)
  // to allow the FIN packet to be sent cleanly during close().
  //
  // Note: This function may be called with an already-closed socket if the network
  // stack closed it. In that case, shutdown() will fail but close() is safe to call.
  //
  // See: https://github.com/esphome/esphome-webserver/issues/163

  AsyncStreamResponse::socket_closing(hd, sockfd);

  // Attempt shutdown - ignore errors as socket may already be closed
  shutdown(sockfd, SHUT_RD);

  // Always close - safe even if socket is already closed by network stack
  close(sockfd);
}

void AsyncWebServer::end() {
  if (this->server_) {
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }
}

void AsyncWebServer::begin() {
  if (this->server_) {
    this->end();
  }
  // Allow room for SerializationBuffer's 640-byte stack buffer and HTTP request handling.
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 4512;
  config.server_port = this->port_;
  config.max_open_sockets = WEB_SERVER_MAX_OPEN_SOCKETS;
  config.uri_match_fn = [](const char * /*unused*/, const char * /*unused*/, size_t /*unused*/) { return true; };
  // Always enable LRU purging to handle socket exhaustion gracefully.
  // When max sockets is reached, the oldest connection is closed to make room for new ones.
  // This prevents "httpd_accept_conn: error in accept (23)" errors.
  // See: https://github.com/esphome/esphome/issues/12464
  config.lru_purge_enable = true;
  // Use custom close function that shuts down before closing to prevent lwIP race conditions
  config.close_fn = AsyncWebServer::safe_close_with_shutdown;
  if (httpd_start(&this->server_, &config) == ESP_OK) {
    const httpd_uri_t handler_get = {
        .uri = "",
        .method = HTTP_GET,
        .handler = AsyncWebServer::request_handler,
        .user_ctx = this,
    };
    httpd_register_uri_handler(this->server_, &handler_get);

    const httpd_uri_t handler_post = {
        .uri = "",
        .method = HTTP_POST,
        .handler = AsyncWebServer::request_post_handler,
        .user_ctx = this,
    };
    httpd_register_uri_handler(this->server_, &handler_post);

    const httpd_uri_t handler_options = {
        .uri = "",
        .method = HTTP_OPTIONS,
        .handler = AsyncWebServer::request_handler,
        .user_ctx = this,
    };
    httpd_register_uri_handler(this->server_, &handler_options);
  }
}

esp_err_t AsyncWebServer::request_post_handler(httpd_req_t *r) {
  ESP_LOGVV(TAG, "Enter AsyncWebServer::request_post_handler. uri=%s", r->uri);
  auto content_type = request_get_header(r, "Content-Type");

  if (!request_has_header(r, "Content-Length")) {
    ESP_LOGW(TAG, "Content length is required for post: %s", r->uri);
    httpd_resp_send_err(r, HTTPD_411_LENGTH_REQUIRED, nullptr);
    return ESP_OK;
  }

  if (content_type.has_value()) {
    const char *content_type_char = content_type.value().c_str();

    // Check most common case first
    size_t content_type_len = strlen(content_type_char);
    if (strcasestr_n(content_type_char, content_type_len, "application/x-www-form-urlencoded") != nullptr) {
      // Normal form data - proceed with regular handling
#ifdef USE_WEBSERVER_OTA
    } else if (strcasestr_n(content_type_char, content_type_len, "multipart/form-data") != nullptr) {
      auto *server = static_cast<AsyncWebServer *>(r->user_ctx);
      return server->handle_multipart_upload_(r, content_type_char);
#endif
    } else {
      // Other content types (e.g. application/json) are delivered raw to a matching
      // custom handler via handleBody(), like the Arduino AsyncWebServer does
      auto *server = static_cast<AsyncWebServer *>(r->user_ctx);
      return server->handle_raw_body_(r, content_type_char);
    }
  }

  // Handle regular form data
  if (r->content_len > CONFIG_HTTPD_MAX_REQ_HDR_LEN) {
    ESP_LOGW(TAG, "Request size is to big: %zu", r->content_len);
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, nullptr);
    return ESP_FAIL;
  }

  std::string post_query;
  if (r->content_len > 0) {
    post_query.resize(r->content_len);
    const int ret = httpd_req_recv(r, &post_query[0], r->content_len + 1);
    if (ret <= 0) {  // 0 return value indicates connection closed
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        httpd_resp_send_err(r, HTTPD_408_REQ_TIMEOUT, nullptr);
        return ESP_ERR_TIMEOUT;
      }
      httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, nullptr);
      return ESP_FAIL;
    }
  }

  AsyncWebServerRequest req(r, std::move(post_query));
  return static_cast<AsyncWebServer *>(r->user_ctx)->request_handler_(&req);
}

esp_err_t AsyncWebServer::request_handler(httpd_req_t *r) {
  ESP_LOGVV(TAG, "Enter AsyncWebServer::request_handler. method=%u, uri=%s", r->method, r->uri);
  AsyncWebServerRequest req(r);
  return static_cast<AsyncWebServer *>(r->user_ctx)->request_handler_(&req);
}

esp_err_t AsyncWebServer::request_handler_(AsyncWebServerRequest *request) const {
  for (auto *handler : this->handlers_) {
    if (handler->canHandle(request)) {
      // At now process only basic requests.
      // OTA requires multipart request support and handleUpload for it
      handler->handleRequest(request);
      return ESP_OK;
    }
  }
  if (this->on_not_found_) {
    this->on_not_found_(request);
    return ESP_OK;
  }
  return ESP_ERR_NOT_FOUND;
}

esp_err_t AsyncWebServer::handle_raw_body_(httpd_req_t *r, const char *content_type) {
  AsyncWebServerRequest req(r);
  AsyncWebHandler *handler = nullptr;
  for (auto *h : this->handlers_) {
    if (h->canHandle(&req)) {
      handler = h;
      break;
    }
  }

  if (handler == nullptr) {
    ESP_LOGW(TAG, "Unsupported content type for POST: %s", content_type);
    // fallback to get handler to support backward compatibility
    return this->request_handler_(&req);
  }

  const size_t total = r->content_len;
  if (total > 0) {
    auto buffer = std::make_unique_for_overwrite<char[]>(RECV_CHUNK_SIZE);
    size_t bytes_since_yield = 0;

    for (size_t index = 0; index < total;) {
      int recv_len = httpd_req_recv(r, buffer.get(), std::min(total - index, RECV_CHUNK_SIZE));

      if (recv_len <= 0) {
        httpd_resp_send_err(r, recv_len == HTTPD_SOCK_ERR_TIMEOUT ? HTTPD_408_REQ_TIMEOUT : HTTPD_400_BAD_REQUEST,
                            nullptr);
        return recv_len == HTTPD_SOCK_ERR_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL;
      }

      handler->handleBody(&req, reinterpret_cast<uint8_t *>(buffer.get()), recv_len, index, total);
      index += recv_len;
      bytes_since_yield += recv_len;

      if (bytes_since_yield > YIELD_INTERVAL_BYTES) {
        vTaskDelay(1);
        bytes_since_yield = 0;
      }
    }
  }

  handler->handleRequest(&req);
  return ESP_OK;
}

AsyncWebServerRequest::~AsyncWebServerRequest() {
#ifdef USE_WEBSERVER_OTA
  if (this->on_disconnect_)
    this->on_disconnect_();
#endif
  delete this->rsp_;
  for (auto *param : this->params_) {
    delete param;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

bool AsyncWebServerRequest::hasHeader(const char *name) const { return request_has_header(*this, name); }

optional<std::string> AsyncWebServerRequest::get_header(const char *name) const {
  return request_get_header(*this, name);
}

size_t AsyncWebServerRequest::get_header_to(const char *name, std::span<char> buf) const {
  return request_get_header_to(*this, name, buf);
}

StringRef AsyncWebServerRequest::url_to(std::span<char, URL_BUF_SIZE> buffer) const {
  const char *uri = this->req_->uri;
  const char *query_start = strchr(uri, '?');
  size_t uri_len = query_start ? static_cast<size_t>(query_start - uri) : strlen(uri);
  size_t copy_len = std::min(uri_len, URL_BUF_SIZE - 1);
  memcpy(buffer.data(), uri, copy_len);
  buffer[copy_len] = '\0';
  // Decode URL-encoded characters in-place (e.g., %20 -> space)
  size_t decoded_len = url_decode(buffer.data());
  return StringRef(buffer.data(), decoded_len);
}

void AsyncWebServerRequest::redirect(const char *url) {
  httpd_resp_set_status(*this, "302 Found");
  httpd_resp_set_hdr(*this, "Location", url);
  httpd_resp_set_hdr(*this, "Connection", "close");
  httpd_resp_send(*this, nullptr, 0);
}

void AsyncWebServerRequest::init_response_(AsyncWebServerResponse *rsp, int code, const char *content_type) {
  // Set status code - use constants for common codes, default to 500 for unknown codes
  const char *status;
  switch (code) {
    case 200:
      status = HTTPD_200;
      break;
    case 204:
      status = HTTPD_204;
      break;
    case 400:
      status = HTTPD_400;
      break;
    case 401:
      status = HTTPD_401;
      break;
    case 403:
      status = HTTPD_403;
      break;
    case 404:
      status = HTTPD_404;
      break;
    case 409:
      status = HTTPD_409;
      break;
    case 422:
      status = HTTPD_422;
      break;
    default:
      status = HTTPD_500;
      break;
  }
  httpd_resp_set_status(*this, status);

  if (content_type && *content_type) {
    httpd_resp_set_type(*this, content_type);
  }
  httpd_resp_set_hdr(*this, "Accept-Ranges", "none");

  // A response object sends the defaults in send(), after its own headers, so one it set itself can
  // replace a default of the same name (httpd_resp_set_hdr only appends).
  if (rsp == nullptr)
    DefaultHeaders::Instance().apply_(*this);

  delete this->rsp_;
  this->rsp_ = rsp;
}

void AsyncWebServerRequest::send(AsyncWebServerResponse *response) {
  DefaultHeaders::Instance().apply_(*this, response->replaced_defaults_);
  httpd_resp_send(*this, response->get_content_data(), response->get_content_size());
}

#ifdef USE_WEBSERVER_AUTH

#ifdef USE_WEBSERVER_AUTH_DIGEST
namespace {

// Extract the value of a Digest auth parameter (e.g. "nonce") from the comma-separated
// parameter list. Values may be quoted or bare. Returns an empty ref when the key is absent.
// Only whole parameter names match, so "nc" does not match inside "cnonce".
StringRef digest_param(StringRef params, const char *key) {
  size_t key_len = strlen(key);
  const char *base = params.c_str();
  size_t n = params.size();
  size_t i = 0;
  while (i < n) {
    while (i < n && (base[i] == ' ' || base[i] == ','))
      i++;
    size_t name_start = i;
    while (i < n && base[i] != '=' && base[i] != ',')
      i++;
    if (i >= n || base[i] == ',')
      continue;  // token without a '=', skip it
    size_t name_len = i - name_start;
    while (name_len > 0 && base[name_start + name_len - 1] == ' ')
      name_len--;
    i++;  // consume '='
    const char *val_start;
    size_t val_len;
    if (i < n && base[i] == '"') {
      i++;
      val_start = base + i;
      while (i < n && base[i] != '"')
        i++;
      val_len = (base + i) - val_start;
      if (i < n)
        i++;  // consume closing quote
    } else {
      val_start = base + i;
      while (i < n && base[i] != ',')
        i++;
      val_len = (base + i) - val_start;
    }
    if (name_len == key_len && memcmp(base + name_start, key, key_len) == 0)
      return StringRef(val_start, val_len);
    while (i < n && base[i] != ',')
      i++;
  }
  return StringRef();
}

// Verify an RFC 2617 Digest response. Stateless (the nonce we issued is not tracked), which
// matches the ESPAsyncWebServer backend used on the Arduino platforms.
bool check_digest_auth(const char *username, const char *password, const std::string &header, const char *method) {
  const size_t prefix_len = sizeof("Digest ") - 1;
  StringRef params(header.c_str() + prefix_len, header.size() - prefix_len);

  if (digest_param(params, "username") != username)
    return false;

  StringRef realm = digest_param(params, "realm");
  StringRef nonce = digest_param(params, "nonce");
  StringRef uri = digest_param(params, "uri");
  StringRef qop = digest_param(params, "qop");
  StringRef nc = digest_param(params, "nc");
  StringRef cnonce = digest_param(params, "cnonce");
  StringRef response = digest_param(params, "response");
  if (response.size() != 32)
    return false;

  // Compute the three MD5 hashes by streaming the pieces straight into the ROM MD5 engine, so
  // nothing is concatenated on the heap. Each hash is emitted as 32 lowercase hex characters.
  md5_context_t ctx;
  uint8_t digest[16];

  // HA1 = MD5(username:realm:password) -- uses the realm the client echoed back.
  char ha1[33];
  esp_rom_md5_init(&ctx);
  esp_rom_md5_update(&ctx, username, strlen(username));
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, realm.c_str(), realm.size());
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, password, strlen(password));
  esp_rom_md5_final(digest, &ctx);
  format_hex_to(ha1, digest, sizeof(digest));

  // HA2 = MD5(method:uri) -- uses the uri the client echoed back.
  char ha2[33];
  esp_rom_md5_init(&ctx);
  esp_rom_md5_update(&ctx, method, strlen(method));
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, uri.c_str(), uri.size());
  esp_rom_md5_final(digest, &ctx);
  format_hex_to(ha2, digest, sizeof(digest));

  // expected = MD5(HA1:nonce:nc:cnonce:qop:HA2)
  char expected[33];
  esp_rom_md5_init(&ctx);
  esp_rom_md5_update(&ctx, ha1, 32);
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, nonce.c_str(), nonce.size());
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, nc.c_str(), nc.size());
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, cnonce.c_str(), cnonce.size());
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, qop.c_str(), qop.size());
  esp_rom_md5_update(&ctx, ":", 1);
  esp_rom_md5_update(&ctx, ha2, 32);
  esp_rom_md5_final(digest, &ctx);
  format_hex_to(expected, digest, sizeof(digest));

  // Constant-time comparison of the two 32-char hex digests.
  uint8_t result = 0;
  for (size_t i = 0; i < 32; i++)
    result |= static_cast<uint8_t>(expected[i] ^ response[i]);
  return result == 0;
}

}  // namespace
#endif  // USE_WEBSERVER_AUTH_DIGEST

bool AsyncWebServerRequest::authenticate(const char *username, const char *password) const {
  if (username == nullptr || password == nullptr || *username == 0) {
    return true;
  }
  auto auth = this->get_header("Authorization");
  if (!auth.has_value()) {
    return false;
  }

  auto *auth_str = auth.value().c_str();

#ifdef USE_WEBSERVER_AUTH_DIGEST
  // The build fixed the scheme to Digest, so the Basic path is compiled out entirely.
  const auto auth_prefix_len = sizeof("Digest ") - 1;
  if (strncmp("Digest ", auth_str, auth_prefix_len) != 0) {
    ESP_LOGW(TAG, "Only Digest authorization supported");
    return false;
  }
  return check_digest_auth(username, password, auth.value(), http_method_str(this->method()));
#else
  const auto auth_prefix_len = sizeof("Basic ") - 1;
  if (strncmp("Basic ", auth_str, auth_prefix_len) != 0) {
    ESP_LOGW(TAG, "Only Basic authorization supported");
    return false;
  }

  // Build user:pass in stack buffer to avoid heap allocation
  constexpr size_t max_user_info_len = 256;
  char user_info[max_user_info_len];
  size_t user_len = strlen(username);
  size_t pass_len = strlen(password);
  size_t user_info_len = user_len + 1 + pass_len;

  if (user_info_len >= max_user_info_len) {
    ESP_LOGW(TAG, "Credentials too long for authentication");
    return false;
  }

  memcpy(user_info, username, user_len);
  user_info[user_len] = ':';
  memcpy(user_info + user_len + 1, password, pass_len);
  user_info[user_info_len] = '\0';

  // Base64 output size is ceil(input_len * 4/3) + 1, with input bounded to 256 bytes
  // max output is ceil(256 * 4/3) + 1 = 343 bytes, use 350 for safety
  constexpr size_t max_digest_len = 350;
  char digest[max_digest_len];
  size_t out;
  // The buffer bound above makes failure unreachable; reject rather than
  // compare against an unwritten digest if that ever changes.
  if (mbedtls_base64_encode(reinterpret_cast<uint8_t *>(digest), max_digest_len, &out,
                            reinterpret_cast<const uint8_t *>(user_info), user_info_len) != 0) {
    return false;
  }

  // Constant-time comparison to avoid timing side channels.
  // No early return on length mismatch — the length difference is folded
  // into the accumulator so any mismatch is rejected.
  const char *provided = auth_str + auth_prefix_len;
  size_t digest_len = out;
  // Derive provided_len from the already-sized std::string rather than
  // rescanning with strlen (avoids attacker-controlled scan length).
  size_t provided_len = auth.value().size() - auth_prefix_len;
  // Use full-width XOR so any bit difference in the lengths is preserved
  // (uint8_t truncation would miss differences in higher bytes, e.g.
  // digest_len vs digest_len + 256).
  volatile size_t result = digest_len ^ provided_len;
  // Iterate over the expected digest length only — the full-width length
  // XOR above already rejects any length mismatch, and bounding the loop
  // prevents a long Authorization header from forcing extra work.
  for (size_t i = 0; i < digest_len; i++) {
    char provided_ch = (i < provided_len) ? provided[i] : 0;
    result |= static_cast<uint8_t>(digest[i] ^ provided_ch);
  }
  return result == 0;
#endif  // USE_WEBSERVER_AUTH_DIGEST
}

void AsyncWebServerRequest::requestAuthentication() const {
  httpd_resp_set_hdr(*this, "Connection", "keep-alive");
#ifdef USE_WEBSERVER_AUTH_DIGEST
  // Issue a fresh random nonce and opaque. The nonce is not stored, so this is stateless and
  // does not defend against replay -- its purpose is to keep the password off the wire.
  // The header value must stay alive until httpd_resp_send_err() below sends it, so the buffer
  // lives on this stack frame (httpd_resp_set_hdr stores the pointer, it does not copy).
  uint8_t random_bytes[16];
  char nonce[33];
  char opaque[33];
  char header[160];
  esp_fill_random(random_bytes, sizeof(random_bytes));
  format_hex_to(nonce, random_bytes, sizeof(random_bytes));
  esp_fill_random(random_bytes, sizeof(random_bytes));
  format_hex_to(opaque, random_bytes, sizeof(random_bytes));
  snprintf(header, sizeof(header), R"(Digest realm="Login Required", qop="auth", nonce="%s", opaque="%s")", nonce,
           opaque);
  httpd_resp_set_hdr(*this, "WWW-Authenticate", header);
#else
  httpd_resp_set_hdr(*this, "WWW-Authenticate", "Basic realm=\"Login Required\"");
#endif  // USE_WEBSERVER_AUTH_DIGEST
  httpd_resp_send_err(*this, HTTPD_401_UNAUTHORIZED, nullptr);
}
#endif  // USE_WEBSERVER_AUTH

AsyncWebParameter *AsyncWebServerRequest::getParam(const char *name) {
  // Check cache first - only successful lookups are cached
  for (auto *param : this->params_) {
    if (param->name() == name) {
      return param;
    }
  }

  // Look up value from query strings
  auto val = this->find_query_value_(name);

  // Don't cache misses to avoid wasting memory when handlers check for
  // optional parameters that don't exist in the request
  if (!val.has_value()) {
    return nullptr;
  }

  auto *param = new AsyncWebParameter(name, val.value());  // NOLINT(cppcoreguidelines-owning-memory)
  this->params_.push_back(param);
  return param;
}

/// Search post_query then URL query with a callback.
/// Returns first truthy result, or value-initialized default.
/// URL query is accessed directly from req->uri (same pattern as url_to()).
template<typename Func>
static auto search_query_sources(httpd_req_t *req, const std::string &post_query, const char *name, Func func)
    -> decltype(func(nullptr, size_t{0}, name)) {
  if (!post_query.empty()) {
    auto result = func(post_query.c_str(), post_query.size(), name);
    if (result) {
      return result;
    }
  }
  // Use httpd API for query length, then access string directly from URI.
  // http_parser identifies components by offset/length without modifying the URI string.
  // This is the same pattern used by url_to().
  auto len = httpd_req_get_url_query_len(req);
  if (len == 0) {
    return {};
  }
  const char *query = strchr(req->uri, '?');
  if (query == nullptr) {
    return {};
  }
  query++;  // skip '?'
  return func(query, len, name);
}

optional<std::string> AsyncWebServerRequest::find_query_value_(const char *name) const {
  return search_query_sources(this->req_, this->post_query_, name,
                              [](const char *q, size_t len, const char *k) { return query_key_value(q, len, k); });
}

bool AsyncWebServerRequest::hasArg(const char *name) {
  return search_query_sources(this->req_, this->post_query_, name, query_has_key);
}

std::string AsyncWebServerRequest::arg(const char *name) {
  auto val = this->find_query_value_(name);
  if (val.has_value()) {
    return std::move(val.value());
  }
  return {};
}

void AsyncWebServerResponse::addHeader(const char *name, const char *value) {
  const auto &defaults = DefaultHeaders::Instance().headers_;
  for (size_t i = 0; i < defaults.size(); i++) {
    if (str_equals_case_insensitive(name, defaults[i].name))
      this->replaced_defaults_ |= 1U << i;
  }
  httpd_resp_set_hdr(*this->req_, name, value);
}

void AsyncResponseStream::print(float value) {
  // Use stack buffer to avoid temporary string allocation
  // Size: sign (1) + digits (10) + decimal (1) + precision (6) + exponent (5) + null (1) = 24, use 32 for safety
  char buf[32];
  int len = snprintf(buf, sizeof(buf), "%f", value);
  this->content_.append(buf, len);
}

void AsyncResponseStream::printf(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  const int length = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);

  std::string str;
  str.resize(length);

  va_start(args, fmt);
  vsnprintf(&str[0], length + 1, fmt, args);
  va_end(args);

  this->print(str);
}

void AsyncStreamResponse::begin(AsyncWebServerRequest *request, const char *status, const char *content_type,
                                std::span<const HttpHeader> headers, const char *body) {
  httpd_req_t *req = *request;
  // Same head httpd_resp_send_chunk() writes, without its first chunk. Blocking sends: the session's non-blocking
  // send override is not installed yet.
  const int fd = httpd_req_to_sockfd(req);
  const auto &defaults = DefaultHeaders::Instance().headers_;
  const bool head_sent = write_stream_head<struct iovec>(
      [fd](struct iovec *iov, int count) {
        while (count > 0) {
          struct msghdr msg{};
          msg.msg_iov = iov;
          msg.msg_iovlen = count;
          const ssize_t sent = sendmsg(fd, &msg, 0);
          if (sent <= 0) {
            return false;
          }
          advance_iovecs(iov, count, sent);
        }
        return true;
      },
      status, content_type, headers, std::span<const HttpHeader>(defaults.data(), defaults.size()), body);

  req->sess_ctx = this;
  req->free_ctx = AsyncStreamResponse::destroy;

  this->hd_ = req->handle;
  this->fd_.store(fd);

  // Use non-blocking send to prevent watchdog timeouts when TCP buffers are full
  httpd_sess_set_send_override(this->hd_, fd, nonblocking_send);

  if (!head_sent) {
    // A partly sent head must not be continued by later writes. close() is main loop only, so shut the socket
    // down here; the first write() then fails and closes the stream, and httpd frees the session as usual.
    shutdown(fd, SHUT_RDWR);
  }
}

// Held around a stream's ownership check and send, and while its socket is marked closed.
// Created at boot, so no allocation can fail once requests arrive.
static Mutex stream_send_mutex;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void AsyncStreamResponse::socket_closing(httpd_handle_t hd, int fd) {
  // Only streams set a session context
  auto *rsp = static_cast<AsyncStreamResponse *>(httpd_sess_get_ctx(hd, fd));
  if (rsp == nullptr)
    return;
  LockGuard guard{stream_send_mutex};
  rsp->socket_closed_ = true;
}

void AsyncStreamResponse::destroy(void *ptr) {
  auto *rsp = static_cast<AsyncStreamResponse *>(ptr);
  int fd = rsp->fd_.exchange(0);  // Atomically get and clear fd
  ESP_LOGD(TAG, "Stream connection closed (fd: %d)", fd);
  // Mark as dead - the owner deletes it from the main loop once safe_to_delete().
  // httpd will call our custom close_fn (safe_close_with_shutdown) which handles
  // shutdown() before close() to prevent lwIP race conditions
}

void AsyncStreamResponse::close() {
  if (!this->close_requested_) {
    this->close_requested_ = true;
    this->next_close_attempt_ms_ = App.get_loop_component_start_time();
  }
  this->process_close_();
}

void AsyncStreamResponse::loop() {
  if (this->close_requested_) {
    this->process_close_();
  } else {
    this->refresh_lru_();
  }
}

void AsyncStreamResponse::process_close_() {
  if (!this->close_requested_ || this->close_work_queued_.load(std::memory_order_acquire)) {
    return;
  }
  const int fd = this->fd_.load();
  if (fd == 0) {
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  if (static_cast<int32_t>(now - this->next_close_attempt_ms_) < 0) {
    return;
  }

  // Queue an identity-checked shutdown on the HTTPD task. The public
  // httpd_sess_trigger_close() queues only a reusable fd/session slot and can
  // therefore close a new client if the original peer disconnects meanwhile.
  this->close_work_queued_.store(true, std::memory_order_release);
  const esp_err_t err = httpd_queue_work(this->hd_, &AsyncStreamResponse::close_session_work, this);
  this->next_close_attempt_ms_ = now + (err == ESP_OK ? CLOSE_CONFIRM_INTERVAL_MS : CLOSE_RETRY_INTERVAL_MS);
  if (err == ESP_OK) {
    return;
  }

  this->close_work_queued_.store(false, std::memory_order_release);
  if (!this->close_retry_warning_logged_) {
    ESP_LOGW(TAG, "Failed to queue stream close (%s); retrying", esp_err_to_name(err));
    this->close_retry_warning_logged_ = true;
  }
}

void AsyncStreamResponse::refresh_lru_() {
  // httpd only advances a session's LRU counter when a request arrives, and a stream receives one
  // request ever, so the purge that frees a socket for a new client would evict the live streams first.
  // The HTTPD task owns the session table, so the refresh runs there.
  // A stalled stream is the socket the purge should take, and a zeroed fd has no session left to refresh.
  if (this->stall_.is_running() || this->fd_.load() == 0)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_lru_refresh_ms_ < LRU_REFRESH_INTERVAL_MS ||
      this->lru_work_queued_.load(std::memory_order_acquire)) {
    return;
  }
  this->last_lru_refresh_ms_ = now;  // a full queue also waits for the next interval
  this->lru_work_queued_.store(true, std::memory_order_release);
  if (httpd_queue_work(this->hd_, &AsyncStreamResponse::refresh_lru_work, this) != ESP_OK) {
    this->lru_work_queued_.store(false, std::memory_order_release);
  }
}

void AsyncStreamResponse::refresh_lru_work(void *arg) {
  auto *response = static_cast<AsyncStreamResponse *>(arg);
  const int fd = response->fd_.load();
  if (response->owns_session_(fd)) {
    httpd_sess_update_lru_counter(response->hd_, fd);
  }
  // Release self only after the HTTPD-task callback has finished every access.
  response->lru_work_queued_.store(false, std::memory_order_release);
}

void AsyncStreamResponse::close_session_work(void *arg) {
  auto *response = static_cast<AsyncStreamResponse *>(arg);
  const int fd = response->fd_.load();
  if (response->owns_session_(fd)) {
    // The HTTPD task remains the session owner. Shutting the socket down makes
    // its next select/recv path delete the session and invoke destroy().
    shutdown(fd, SHUT_RDWR);
  }

  // Release self only after the HTTPD-task callback has finished every access.
  response->close_work_queued_.store(false, std::memory_order_release);
}

ssize_t AsyncStreamResponse::write(struct iovec *iov, int iovcnt) {
  if (this->close_requested_) {
    return -1;
  }
  // Nothing to send is not a stall
  size_t total = 0;
  for (int i = 0; i < iovcnt; i++) {
    total += iov[i].iov_len;
  }
  if (total == 0) {
    return 0;
  }
  // httpd closes the socket before it clears the session, so the fd number may already be a new client's.
  // Treated as would-block; the stall timer ends a session that never becomes ours again.
  const int fd = this->fd_.load();
  ssize_t sent = 0;
  int err = 0;
  {
    // Held across the check and the send, so socket_closing() cannot close the socket in between
    LockGuard guard{stream_send_mutex};
    if (!this->socket_closed_ && this->owns_session_(fd)) {
      struct msghdr msg{};
      msg.msg_iov = iov;
      msg.msg_iovlen = iovcnt;
      sent = sendmsg(fd, &msg, MSG_DONTWAIT);
      err = errno;
    }
  }
  if (sent > 0) {
    this->stall_.clear();
    return sent;
  }
  if (sent < 0 && err != EAGAIN && err != EWOULDBLOCK) {
    ESP_LOGD(TAG, "send error: errno %d", err);
    // A hard error is not a timeout, so is_stalled() must not report one
    this->stall_.clear();
    this->close();
    return -1;
  }
  // Socket buffer full, try again later; a socket that stays full past the stall timeout is closed
  return this->stall_or_close_(LOG_STR("without send progress")) ? -1 : 0;
}

bool AsyncStreamResponse::stall_or_close_(const LogString *reason) {
  const uint32_t now = App.get_loop_component_start_time();
  this->stall_.start(now);
  if (!this->stall_.is_expired(now, SEND_STALL_TIMEOUT_MS)) {
    return false;
  }
  ESP_LOGW(TAG, "Closing stuck stream after %" PRIu32 " ms %s", this->stall_.elapsed(now), LOG_STR_ARG(reason));
  this->close();
  return true;
}

ssize_t AsyncStreamResponse::write(const void *data, size_t len) {
  struct iovec iov = {const_cast<void *>(data), len};
  return this->write(&iov, 1);
}

#ifdef USE_WEBSERVER_OTA
esp_err_t AsyncWebServer::handle_multipart_upload_(httpd_req_t *r, const char *content_type) {
  // Parse boundary and create reader
  const char *boundary_start;
  size_t boundary_len;
  if (!parse_multipart_boundary(content_type, &boundary_start, &boundary_len)) {
    ESP_LOGE(TAG, "Failed to parse multipart boundary");
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, nullptr);
    return ESP_FAIL;
  }

  AsyncWebServerRequest req(r);
  AsyncWebHandler *handler = nullptr;
  for (auto *h : this->handlers_) {
    if (h->canHandle(&req)) {
      handler = h;
      break;
    }
  }

  if (!handler) {
    ESP_LOGW(TAG, "No handler found for OTA request");
    httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, nullptr);
    return ESP_OK;
  }

  // Upload state
  std::string filename;
  size_t index = 0;
  // Create reader on heap to reduce stack usage
  auto reader = std::make_unique<MultipartReader>("--" + std::string(boundary_start, boundary_len));

  // Configure callbacks
  reader->set_data_callback([&](const uint8_t *data, size_t len) {
    if (!reader->has_file() || !len)
      return;

    if (filename.empty()) {
      filename = reader->get_current_part().filename;
      ESP_LOGV(TAG, "Processing file: '%s'", filename.c_str());
      handler->handleUpload(&req, filename, 0, nullptr, 0, false);  // Start
    }

    handler->handleUpload(&req, filename, index, const_cast<uint8_t *>(data), len, false);
    index += len;
  });

  reader->set_part_complete_callback([&]() {
    if (index > 0) {
      handler->handleUpload(&req, filename, index, nullptr, 0, true);  // End
      filename.clear();
      index = 0;
    }
  });

  auto buffer = std::make_unique_for_overwrite<char[]>(RECV_CHUNK_SIZE);
  size_t bytes_since_yield = 0;

  for (size_t remaining = r->content_len; remaining > 0;) {
    int recv_len = httpd_req_recv(r, buffer.get(), std::min(remaining, RECV_CHUNK_SIZE));

    if (recv_len <= 0) {
      httpd_resp_send_err(r, recv_len == HTTPD_SOCK_ERR_TIMEOUT ? HTTPD_408_REQ_TIMEOUT : HTTPD_400_BAD_REQUEST,
                          nullptr);
      return recv_len == HTTPD_SOCK_ERR_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL;
    }

    if (reader->parse(buffer.get(), recv_len) != static_cast<size_t>(recv_len)) {
      ESP_LOGW(TAG, "Multipart parser error");
      httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, nullptr);
      return ESP_FAIL;
    }

    remaining -= recv_len;
    bytes_since_yield += recv_len;

    if (bytes_since_yield > YIELD_INTERVAL_BYTES) {
      vTaskDelay(1);
      bytes_since_yield = 0;
    }
  }

  handler->handleRequest(&req);
  return ESP_OK;
}
#endif  // USE_WEBSERVER_OTA

}  // namespace esphome::web_server_idf

#endif  // !defined(USE_ESP32)

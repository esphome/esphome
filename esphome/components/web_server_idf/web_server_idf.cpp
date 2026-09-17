#ifdef USE_ESP32

#include <cstdarg>
#include <memory>
#include <cstring>
#include <cctype>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_tls_crypto.h"
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

#ifdef USE_WEBSERVER
#include "esphome/components/web_server/web_server.h"
#include "esphome/components/web_server/list_entities.h"
#endif  // USE_WEBSERVER

// Include socket headers after Arduino headers to avoid IPADDR_NONE/INADDR_NONE macro conflicts
#include <cerrno>
#include <sys/socket.h>

namespace esphome::web_server_idf {

// Status strings not provided by esp_http_server.h
#ifndef HTTPD_401
#define HTTPD_401 "401 Unauthorized"
#endif
#ifndef HTTPD_409
#define HTTPD_409 "409 Conflict"
#endif
#ifndef HTTPD_422
#define HTTPD_422 "422 Unprocessable Entity"
#endif

#define CRLF_STR "\r\n"
#define CRLF_LEN (sizeof(CRLF_STR) - 1)

static const char *const TAG = "web_server_idf";

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
  // Default httpd stack is defined by ESP-IDF. Increase to accommodate SerializationBuffer's
  // 640-byte stack buffer used by web_server JSON request handlers.
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = config.stack_size + 256;
  config.server_port = this->port_;
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
  delete this->rsp_;
  for (auto *param : this->params_) {
    delete param;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

bool AsyncWebServerRequest::hasHeader(const char *name) const { return request_has_header(*this, name); }

optional<std::string> AsyncWebServerRequest::get_header(const char *name) const {
  return request_get_header(*this, name);
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

void AsyncWebServerRequest::redirect(const std::string &url) {
  httpd_resp_set_status(*this, "302 Found");
  httpd_resp_set_hdr(*this, "Location", url.c_str());
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

  for (const auto &header : DefaultHeaders::Instance().headers_) {
    httpd_resp_set_hdr(*this, header.name, header.value);
  }

  delete this->rsp_;
  this->rsp_ = rsp;
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
  esp_crypto_base64_encode(reinterpret_cast<uint8_t *>(digest), max_digest_len, &out,
                           reinterpret_cast<const uint8_t *>(user_info), user_info_len);

  // Constant-time comparison to avoid timing side channels.
  // No early return on length mismatch — the length difference is folded
  // into the accumulator so any mismatch is rejected.
  const char *provided = auth_str + auth_prefix_len;
  size_t digest_len = out;  // length from esp_crypto_base64_encode
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

#ifdef USE_WEBSERVER
namespace {

// HTTP chunk header "%08x\r\n"
constexpr size_t CHUNK_HDR_LEN = 10;
// Between two data lines: the end of one and the prefix of the next
constexpr char SSE_SEP[] = "\r\ndata: ";
constexpr size_t SSE_SEP_LEN = sizeof(SSE_SEP) - 1;
// End of the last data line, the blank line ending the event, and the chunk terminator
constexpr char SSE_SUFFIX[] = "\r\n\r\n\r\n";
constexpr size_t SSE_SUFFIX_LEN = sizeof(SSE_SUFFIX) - 1;
// The suffix bytes that count toward the chunk length (the terminator does not)
constexpr size_t SSE_SUFFIX_BODY_LEN = SSE_SUFFIX_LEN - CRLF_LEN;

// Splits a message into SSE data lines on \n, \r or \r\n (matching ESPAsyncWebServer).
// A trailing line break does not yield an empty last line; an inner empty line is kept.
struct SseLineIter {
  const char *pos;
  const char *end;
  bool done{false};

  bool next(const char *&line, size_t &len) {
    if (this->done) {
      return false;
    }
    const size_t remaining = this->end - this->pos;
    const auto *n = static_cast<const char *>(memchr(this->pos, '\n', remaining));
    const auto *r = static_cast<const char *>(memchr(this->pos, '\r', remaining));
    line = this->pos;
    if (n == nullptr && r == nullptr) {
      len = remaining;
      this->done = true;
      return true;
    }
    const char *brk = (r != nullptr && (n == nullptr || r < n)) ? r : n;
    len = brk - this->pos;
    this->pos = brk + ((brk == r && brk + 1 == n) ? 2 : 1);
    this->done = this->pos >= this->end;
    return true;
  }
};

// Copy the bytes of a gather list from offset skip onward into dst
void copy_unsent(uint8_t *dst, const struct iovec *iov, int iovcnt, size_t skip) {
  for (const struct iovec *end = iov + iovcnt; iov != end; iov++) {
    if (skip >= iov->iov_len) {
      skip -= iov->iov_len;
      continue;
    }
    const size_t len = iov->iov_len - skip;
    std::memcpy(dst, static_cast<const uint8_t *>(iov->iov_base) + skip, len);
    dst += len;
    skip = 0;
  }
}

}  // namespace

AsyncEventSource::~AsyncEventSource() {
  LockGuard guard{this->pending_mutex_};
  for (auto *vec : {&this->sessions_, &this->pending_sessions_}) {
    for (auto *ses : *vec) {
      delete ses;  // NOLINT(cppcoreguidelines-owning-memory)
    }
  }
}

void AsyncEventSource::handleRequest(AsyncWebServerRequest *request) {
  // Httpd task: set up the live httpd_req_t and park the session; main loop does the rest.
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,clang-analyzer-cplusplus.NewDeleteLeaks)
  auto *rsp = new AsyncEventSourceResponse(request, this, this->web_server_);
  {
    LockGuard guard{this->pending_mutex_};
    this->pending_sessions_.push_back(rsp);
    this->has_pending_sessions_.store(true, std::memory_order_release);
  }
  this->web_server_->enable_loop_soon_any_context();
}

// clang-analyzer traces a false-positive leak path from loop() through
// adopt_pending_sessions_main_loop_() into start_session_main_loop_() and
// finally ArduinoJson. Suppress along the entire in-our-code call chain.
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
bool AsyncEventSource::loop() {
  // Fast path: one atomic load per tick. Slow path is out-of-line on connect.
  if (this->has_pending_sessions_.load(std::memory_order_acquire)) {
    this->adopt_pending_sessions_main_loop_();
  }

  // Clean up dead sessions safely
  // This follows the ESP-IDF pattern where free_ctx marks resources as dead
  // and the main loop handles the actual cleanup to avoid race conditions
  for (size_t i = 0; i < this->sessions_.size();) {
    auto *ses = this->sessions_[i];
    // If the session has a dead socket (marked by destroy callback)
    if (ses->safe_to_delete_()) {
      // destroy() already logged the close with the fd; don't double-log here.
      delete ses;  // NOLINT(cppcoreguidelines-owning-memory)
      // Remove by swapping with last element (O(1) removal, order doesn't matter for sessions)
      this->sessions_[i] = this->sessions_.back();
      this->sessions_.pop_back();
    } else {
      ses->loop();
      ++i;
    }
  }
  return !this->sessions_.empty();
}

void AsyncEventSource::adopt_pending_sessions_main_loop_() {
  std::vector<AsyncEventSourceResponse *> incoming;
  {
    LockGuard guard{this->pending_mutex_};
    incoming.swap(this->pending_sessions_);
    this->has_pending_sessions_.store(false, std::memory_order_relaxed);
  }
  for (auto *rsp : incoming) {
    // Already disconnected? Drop it; skip on_connect_/session start on a dead session.
    if (rsp->safe_to_delete_()) {
      delete rsp;  // NOLINT(cppcoreguidelines-owning-memory)
      continue;
    }
    this->sessions_.push_back(rsp);
    // Prime first so on_connect_ observes a session that has already sent its
    // initial ping/config/sorting_groups, matching the pre-refactor ordering.
    rsp->start_session_main_loop_();
    if (this->on_connect_) {
      this->on_connect_(rsp);
    }
  }
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void AsyncEventSource::try_send_nodefer(const char *message, size_t message_len, const char *event, uint32_t id,
                                        uint32_t reconnect) {
  for (auto *ses : this->sessions_) {
    if (ses->fd_.load() != 0) {  // Skip dead sessions
      ses->try_send_nodefer(message, message_len, event, id, reconnect);
    }
  }
}

void AsyncEventSource::deferrable_send_state(void *source, const char *event_type,
                                             message_generator_t *message_generator) {
  // Skip if no connected clients to avoid unnecessary processing
  if (this->empty())
    return;
  for (auto *ses : this->sessions_) {
    if (ses->fd_.load() != 0) {  // Skip dead sessions
      ses->deferrable_send_state(source, event_type, message_generator);
    }
  }
}

AsyncEventSourceResponse::AsyncEventSourceResponse(const AsyncWebServerRequest *request,
                                                   esphome::web_server_idf::AsyncEventSource *server,
                                                   esphome::web_server::WebServer *ws)
    : server_(server), web_server_(ws), entities_iterator_(ws, server) {
  // Httpd task only. start_session_main_loop_() sends the greeting and starts the iterator.
  httpd_req_t *req = *request;

  httpd_resp_set_status(req, HTTPD_200);
  httpd_resp_set_type(req, "text/event-stream");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");

  for (const auto &header : DefaultHeaders::Instance().headers_) {
    httpd_resp_set_hdr(req, header.name, header.value);
  }

  httpd_resp_send_chunk(req, CRLF_STR, CRLF_LEN);

  req->sess_ctx = this;
  req->free_ctx = AsyncEventSourceResponse::destroy;

  this->hd_ = req->handle;
  this->fd_.store(httpd_req_to_sockfd(req));

  // Use non-blocking send to prevent watchdog timeouts when TCP buffers are full
  httpd_sess_set_send_override(this->hd_, this->fd_.load(), nonblocking_send);
}

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
void AsyncEventSourceResponse::start_session_main_loop_() {
  auto *ws = this->web_server_;

  // tcp send buffer is empty on connect, so these should always go through
  auto message = ws->get_config_json();
  this->try_send_nodefer(message.c_str(), message.size(), "ping", millis(), 30000);

#ifdef USE_WEBSERVER_SORTING
  for (auto &group : ws->sorting_groups_) {
    json::JsonBuilder builder;
    JsonObject root = builder.root();
    root["name"] = group.second.name;
    root["sorting_weight"] = group.second.weight;
    message = builder.serialize();

    // a (very) large number of these should be able to be queued initially without defer
    // since the only thing in the send buffer at this point is the initial ping/config
    this->try_send_nodefer(message.c_str(), message.size(), "sorting_group");
  }
#endif

  this->entities_iterator_.begin(ws->include_internal_);
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void AsyncEventSourceResponse::destroy(void *ptr) {
  auto *rsp = static_cast<AsyncEventSourceResponse *>(ptr);
  int fd = rsp->fd_.exchange(0);  // Atomically get and clear fd
  ESP_LOGD(TAG, "Event source connection closed (fd: %d)", fd);
  // Mark as dead - will be cleaned up in the main loop
  // Note: We don't delete or remove from set here to avoid race conditions
  // httpd will call our custom close_fn (safe_close_with_shutdown) which handles
  // shutdown() before close() to prevent lwIP race conditions
}

// helper for allowing only unique entries in the queue
void AsyncEventSourceResponse::deq_push_back_with_dedup_(void *source, message_generator_t *message_generator) {
  DeferredEvent item(source, message_generator);

  // Use range-based for loop instead of std::find_if to reduce template instantiation overhead and binary size
  for (auto &event : this->deferred_queue_) {
    if (event == item) {
      return;  // Already in queue, no need to update since items are equal
    }
  }
  this->deferred_queue_.push_back(item);
}

void AsyncEventSourceResponse::process_deferred_queue_() {
  if (this->close_requested_) {
    return;
  }
  while (!deferred_queue_.empty()) {
    DeferredEvent &de = deferred_queue_.front();
    auto message = de.message_generator_(web_server_, de.source_);
    if (this->try_send_nodefer(message.c_str(), message.size(), "state")) {
      if (this->close_requested_ || deferred_queue_.empty()) {
        return;
      }
      // O(n) but memory efficiency is more important than speed here which is why std::vector was chosen
      deferred_queue_.erase(deferred_queue_.begin());
    } else {
      break;
    }
  }
}

void AsyncEventSourceResponse::request_close_() {
  if (!this->close_requested_) {
    this->close_requested_ = true;
    this->deferred_queue_.clear();
    this->tail_.reset();
    this->tail_cap_ = 0;
    this->tail_len_ = 0;
    this->tail_sent_ = 0;
    this->next_close_attempt_ms_ = App.get_loop_component_start_time();
  }

  this->process_close_();
}

void AsyncEventSourceResponse::process_close_() {
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
  const esp_err_t err = httpd_queue_work(this->hd_, &AsyncEventSourceResponse::close_session_work, this);
  this->next_close_attempt_ms_ = now + (err == ESP_OK ? CLOSE_CONFIRM_INTERVAL_MS : CLOSE_RETRY_INTERVAL_MS);
  if (err == ESP_OK) {
    return;
  }

  this->close_work_queued_.store(false, std::memory_order_release);
  if (!this->close_retry_warning_logged_) {
    ESP_LOGW(TAG, "Failed to queue EventSource close (%s); retrying", esp_err_to_name(err));
    this->close_retry_warning_logged_ = true;
  }
}

void AsyncEventSourceResponse::close_session_work(void *arg) {
  auto *response = static_cast<AsyncEventSourceResponse *>(arg);
  const int fd = response->fd_.load();
  if (fd != 0 && httpd_sess_get_ctx(response->hd_, fd) == response) {
    // The HTTPD task remains the session owner. Shutting the socket down makes
    // its next select/recv path delete the session and invoke destroy().
    shutdown(fd, SHUT_RDWR);
  }

  // Release self only after the HTTPD-task callback has finished every access.
  response->close_work_queued_.store(false, std::memory_order_release);
}

void AsyncEventSourceResponse::drain_tail_() {
  if (this->sending_ || this->close_requested_ || this->tail_len_ == 0) {
    return;
  }
  struct SendGuard {
    AsyncEventSourceResponse &owner;
    ~SendGuard() { this->owner.sending_ = false; }
  } guard{*this};
  this->sending_ = true;

  const size_t remaining = this->tail_len_ - this->tail_sent_;
  const char *data = reinterpret_cast<const char *>(this->tail_.get()) + this->tail_sent_;
  int bytes_sent = httpd_socket_send(this->hd_, this->fd_.load(), data, remaining, 0);
  if (bytes_sent == HTTPD_SOCK_ERR_TIMEOUT) {
    // EAGAIN/EWOULDBLOCK - socket buffer full, try again later
    // NOTE: Similar logic exists in web_server/web_server.cpp in DeferredUpdateEventSource::process_deferred_queue_().
    // The IDF path is intentionally time-based and closes through HTTPD to preserve session ownership.
    const uint32_t now = App.get_loop_component_start_time();
    if (this->send_failure_started_ms_ == 0) {
      this->send_failure_started_ms_ = now != 0 ? now : 1;  // Reserve zero for no stall.
    }
    if (static_cast<int32_t>(now - (this->send_failure_started_ms_ + SEND_STALL_TIMEOUT_MS)) >= 0) {
      ESP_LOGW(TAG, "Closing stuck EventSource connection after %" PRIu32 " ms without send progress",
               now - this->send_failure_started_ms_);
      this->request_close_();
    }
    return;
  }
  if (bytes_sent == HTTPD_SOCK_ERR_FAIL) {
    // Low-level asynchronous sends do not make HTTPD close the session automatically.
    this->request_close_();
    return;
  }
  if (bytes_sent <= 0) {
    // Unexpected error or zero bytes sent
    ESP_LOGW(TAG, "Unexpected send result: %d", bytes_sent);
    this->request_close_();
    return;
  }

  // Successful send - reset stall tracking
  this->send_failure_started_ms_ = 0;
  this->tail_sent_ += bytes_sent;
  if (this->tail_sent_ < this->tail_len_) {
    ESP_LOGV(TAG, "Partial send: %d/%zu bytes (total: %u/%u)", bytes_sent, remaining, this->tail_sent_,
             this->tail_len_);
    return;
  }
  // Fully sent; the storage stays for the next stall
  this->tail_len_ = 0;
  this->tail_sent_ = 0;
}

bool AsyncEventSourceResponse::reserve_tail_(size_t len) {
  if (len > UINT16_MAX) {
    return false;
  }
  if (this->tail_cap_ >= len) {
    return true;
  }
  // Nothing is pending when the tail grows, so free the old block before asking for the new one.
  // PREFER_INTERNAL keeps the tail where plain new put it.
  this->tail_.reset();
  this->tail_cap_ = 0;
  this->tail_ = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::PREFER_INTERNAL).make_unique_array_for_overwrite(len);
  if (!this->tail_) {
    return false;
  }
  this->tail_cap_ = len;
  return true;
}

bool AsyncEventSourceResponse::send_from_tail_(const char *prefix, size_t prefix_len, const char *message,
                                               size_t message_len, size_t total) {
  if (!this->reserve_tail_(total)) {
    ESP_LOGW(TAG, "EventSource tail allocation failed (%zu bytes); closing", total);
    this->request_close_();
    return false;
  }
  uint8_t *dst = this->tail_.get();
  std::memcpy(dst, prefix, prefix_len);
  dst += prefix_len;
  SseLineIter it{message, message + message_len};
  const char *line;
  size_t len;
  while (it.next(line, len)) {
    std::memcpy(dst, line, len);
    dst += len;
    const char *sep = it.done ? SSE_SUFFIX : SSE_SEP;
    const size_t sep_len = it.done ? SSE_SUFFIX_LEN : SSE_SEP_LEN;
    std::memcpy(dst, sep, sep_len);
    dst += sep_len;
  }
  this->tail_len_ = total;
  this->tail_sent_ = 0;
  return true;
}

void AsyncEventSourceResponse::loop() {
  if (this->close_requested_) {
    this->process_close_();
    return;
  }
  drain_tail_();
  process_deferred_queue_();
  if (this->close_requested_)
    return;
  // One step per loop; refusals retry next pass
  this->entities_iterator_.try_advance(1);
}

bool AsyncEventSourceResponse::try_send_nodefer(const char *message, size_t message_len, const char *event, uint32_t id,
                                                uint32_t reconnect) {
  // Re-entered from a log line emitted inside a send on this session; see sending_
  if (this->sending_) {
    return false;
  }
  const int fd = this->fd_.load();
  if (fd == 0 || this->close_requested_) {
    return false;
  }

  drain_tail_();
  if (this->close_requested_ || this->tail_len_ != 0) {
    // there is still pending event data to send first
    return false;
  }

  struct SendGuard {
    AsyncEventSourceResponse &owner;
    ~SendGuard() { this->owner.sending_ = false; }
  } guard{*this};
  this->sending_ = true;

  // Chunk header placeholder (formatted once the length is known), the retry/id/event lines and
  // the first data line prefix. Everything else goes out straight from the caller's buffer.
  char prefix[PREFIX_BUF_SIZE];
  size_t prefix_len = CHUNK_HDR_LEN;
  if (reconnect) {
    prefix_len = buf_append_printf(prefix, sizeof(prefix), prefix_len, "retry: %" PRIu32 CRLF_STR, reconnect);
  }
  if (id) {
    prefix_len = buf_append_printf(prefix, sizeof(prefix), prefix_len, "id: %" PRIu32 CRLF_STR, id);
  }
  if (event && *event) {
    prefix_len = buf_append_printf(prefix, sizeof(prefix), prefix_len, "event: %s" CRLF_STR, event);
  }
  if (message) {
    prefix_len = buf_append_str(prefix, sizeof(prefix), prefix_len, "data: ");
  }

  // SSE spec requires each line of a multi-line message to have its own "data:" prefix.
  // Count the lines first so the chunk length is known before anything is written.
  size_t lines = 0;
  size_t line_bytes = 0;
  size_t first_len = 0;
  if (message) {
    SseLineIter it{message, message + message_len};
    const char *line;
    size_t len;
    while (it.next(line, len)) {
      if (lines++ == 0) {
        first_len = len;
      }
      line_bytes += len;
    }
  }
  size_t chunk_len = prefix_len - CHUNK_HDR_LEN + line_bytes;
  if (lines != 0) {
    chunk_len += (lines - 1) * SSE_SEP_LEN + SSE_SUFFIX_BODY_LEN;
  }
  if (chunk_len == 0) {
    // Match ESPAsyncWebServer: a null message with no fields sends nothing
    return true;
  }
  const size_t total = CHUNK_HDR_LEN + chunk_len + CRLF_LEN;
  if (total > UINT16_MAX) {
    return true;  // Beyond what the tail can hold; no caller produces this
  }
  // The temp keeps snprintf's terminator out of the prefix
  char hdr[CHUNK_HDR_LEN + 1];
  snprintf(hdr, sizeof(hdr), "%08x" CRLF_STR, static_cast<unsigned>(chunk_len));
  std::memcpy(prefix, hdr, CHUNK_HDR_LEN);

  if (1 + 2 * lines > MAX_SEND_IOV) {
    return this->send_from_tail_(prefix, prefix_len, message, message_len, total);
  }

  struct iovec iov[MAX_SEND_IOV];
  int iovcnt = 0;
  iov[iovcnt++] = {prefix, prefix_len};
  if (lines == 0) {
    // Null message: no data line and no blank line, only the chunk terminator
    iov[iovcnt++] = {const_cast<char *>(SSE_SUFFIX + SSE_SUFFIX_BODY_LEN), CRLF_LEN};
  } else if (lines == 1) {
    if (first_len != 0) {
      iov[iovcnt++] = {const_cast<char *>(message), first_len};
    }
    iov[iovcnt++] = {const_cast<char *>(SSE_SUFFIX), SSE_SUFFIX_LEN};
  } else {
    SseLineIter it{message, message + message_len};
    const char *line;
    size_t len;
    while (it.next(line, len)) {
      if (len != 0) {
        iov[iovcnt++] = {const_cast<char *>(line), len};
      }
      if (it.done) {
        iov[iovcnt++] = {const_cast<char *>(SSE_SUFFIX), SSE_SUFFIX_LEN};
      } else {
        iov[iovcnt++] = {const_cast<char *>(SSE_SEP), SSE_SEP_LEN};
      }
    }
  }

  // Same non-blocking send as nonblocking_send(), as one gather write
  struct msghdr msg {};
  msg.msg_iov = iov;
  msg.msg_iovlen = iovcnt;
  ssize_t sent = sendmsg(fd, &msg, MSG_DONTWAIT);
  if (sent == static_cast<ssize_t>(total)) [[likely]] {
    this->send_failure_started_ms_ = 0;
    return true;
  }
  if (sent < 0) {
    const int err = errno;
    if (err != EAGAIN && err != EWOULDBLOCK) {
      ESP_LOGD(TAG, "sendmsg error: errno %d", err);
      this->request_close_();
      return false;
    }
    sent = 0;
  } else if (sent > 0) {
    this->send_failure_started_ms_ = 0;
  }

  // Keep what the socket did not take; the caller's buffers do not outlive this call
  const size_t unsent = total - sent;
  if (!this->reserve_tail_(unsent)) {
    ESP_LOGW(TAG, "EventSource tail allocation failed (%zu bytes); closing", unsent);
    this->request_close_();
    return false;
  }
  copy_unsent(this->tail_.get(), iov, iovcnt, sent);
  this->tail_len_ = unsent;
  this->tail_sent_ = 0;
  return true;
}

void AsyncEventSourceResponse::deferrable_send_state(void *source, const char *event_type,
                                                     message_generator_t *message_generator) {
  // allow all json "details_all" to go through before publishing bare state events, this avoids unnamed entries showing
  // up in the web GUI and reduces event load during initial connect
  if (!this->entities_iterator_.completed() && 0 != strcmp(event_type, "state_detail_all"))
    return;

  if (source == nullptr)
    return;
  if (event_type == nullptr)
    return;
  if (message_generator == nullptr)
    return;

  if (0 != strcmp(event_type, "state_detail_all") && 0 != strcmp(event_type, "state")) {
    ESP_LOGE(TAG, "Can't defer non-state event");
  }

  drain_tail_();
  process_deferred_queue_();

  if (this->close_requested_) {
    return;
  }

  if (this->tail_len_ != 0 || !deferred_queue_.empty()) {
    // outgoing event buffer or deferred queue still not empty which means downstream tcp send buffer full, no point
    // trying to send first
    deq_push_back_with_dedup_(source, message_generator);
  } else {
    auto message = message_generator(web_server_, source);
    if (!this->try_send_nodefer(message.c_str(), message.size(), "state")) {
      deq_push_back_with_dedup_(source, message_generator);
    }
  }
}
#endif

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

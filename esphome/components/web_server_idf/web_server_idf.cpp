#ifdef USE_ESP_IDF

#include <cstdarg>
#include <memory>
#include <cstring>
#include <cctype>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_tls_crypto.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "utils.h"
#include "web_server_idf.h"

#ifdef USE_WEBSERVER_OTA
#include <multipart_parser.h>
#include "multipart.h"  // For parse_multipart_boundary and other utils
#endif

#ifdef USE_WEBSERVER
#include "esphome/components/web_server/web_server.h"
#include "esphome/components/web_server/list_entities.h"
#endif  // USE_WEBSERVER

namespace esphome {
namespace web_server_idf {

#ifndef HTTPD_409
#define HTTPD_409 "409 Conflict"
#endif

#define CRLF_STR "\r\n"
#define CRLF_LEN (sizeof(CRLF_STR) - 1)

static const char *const TAG = "web_server_idf";

// Global instance to avoid guard variable (saves 8 bytes)
// This is initialized at program startup before any threads
namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
DefaultHeaders default_headers_instance;
}  // namespace

DefaultHeaders &DefaultHeaders::Instance() { return default_headers_instance; }

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
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.uri_match_fn = [](const char * /*unused*/, const char * /*unused*/, size_t /*unused*/) { return true; };
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
    if (stristr(content_type_char, "application/x-www-form-urlencoded") != nullptr) {
      // Normal form data - proceed with regular handling
#ifdef USE_WEBSERVER_OTA
    } else if (stristr(content_type_char, "multipart/form-data") != nullptr) {
      auto *server = static_cast<AsyncWebServer *>(r->user_ctx);
      return server->handle_multipart_upload_(r, content_type_char);
#endif
    } else {
      ESP_LOGW(TAG, "Unsupported content type for POST: %s", content_type_char);
      // fallback to get handler to support backward compatibility
      return AsyncWebServer::request_handler(r);
    }
  }

  // Handle regular form data
  if (r->content_len > HTTPD_MAX_REQ_HDR_LEN) {
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

AsyncWebServerRequest::~AsyncWebServerRequest() {
  delete this->rsp_;
  for (const auto &pair : this->params_) {
    delete pair.second;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

bool AsyncWebServerRequest::hasHeader(const char *name) const { return request_has_header(*this, name); }

optional<std::string> AsyncWebServerRequest::get_header(const char *name) const {
  return request_get_header(*this, name);
}

std::string AsyncWebServerRequest::url() const {
  auto *str = strchr(this->req_->uri, '?');
  if (str == nullptr) {
    return this->req_->uri;
  }
  return std::string(this->req_->uri, str - this->req_->uri);
}

std::string AsyncWebServerRequest::host() const { return this->get_header("Host").value(); }

void AsyncWebServerRequest::send(AsyncWebServerResponse *response) {
  httpd_resp_send(*this, response->get_content_data(), response->get_content_size());
}

void AsyncWebServerRequest::send(int code, const char *content_type, const char *content) {
  this->init_response_(nullptr, code, content_type);
  if (content) {
    httpd_resp_send(*this, content, HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send(*this, nullptr, 0);
  }
}

void AsyncWebServerRequest::redirect(const std::string &url) {
  httpd_resp_set_status(*this, "302 Found");
  httpd_resp_set_hdr(*this, "Location", url.c_str());
  httpd_resp_send(*this, nullptr, 0);
}

void AsyncWebServerRequest::init_response_(AsyncWebServerResponse *rsp, int code, const char *content_type) {
  httpd_resp_set_status(*this, code == 200   ? HTTPD_200
                               : code == 404 ? HTTPD_404
                               : code == 409 ? HTTPD_409
                                             : to_string(code).c_str());

  if (content_type && *content_type) {
    httpd_resp_set_type(*this, content_type);
  }
  httpd_resp_set_hdr(*this, "Accept-Ranges", "none");

  for (const auto &pair : DefaultHeaders::Instance().headers_) {
    httpd_resp_set_hdr(*this, pair.first.c_str(), pair.second.c_str());
  }

  delete this->rsp_;
  this->rsp_ = rsp;
}

bool AsyncWebServerRequest::authenticate(const char *username, const char *password) const {
  if (username == nullptr || password == nullptr || *username == 0) {
    return true;
  }
  auto auth = this->get_header("Authorization");
  if (!auth.has_value()) {
    return false;
  }

  auto *auth_str = auth.value().c_str();

  const auto auth_prefix_len = sizeof("Basic ") - 1;
  if (strncmp("Basic ", auth_str, auth_prefix_len) != 0) {
    ESP_LOGW(TAG, "Only Basic authorization supported yet");
    return false;
  }

  std::string user_info;
  user_info += username;
  user_info += ':';
  user_info += password;

  size_t n = 0, out;
  esp_crypto_base64_encode(nullptr, 0, &n, reinterpret_cast<const uint8_t *>(user_info.c_str()), user_info.size());

  auto digest = std::unique_ptr<char[]>(new char[n + 1]);
  esp_crypto_base64_encode(reinterpret_cast<uint8_t *>(digest.get()), n, &out,
                           reinterpret_cast<const uint8_t *>(user_info.c_str()), user_info.size());

  return strncmp(digest.get(), auth_str + auth_prefix_len, auth.value().size() - auth_prefix_len) == 0;
}

void AsyncWebServerRequest::requestAuthentication(const char *realm) const {
  httpd_resp_set_hdr(*this, "Connection", "keep-alive");
  auto auth_val = str_sprintf("Basic realm=\"%s\"", realm ? realm : "Login Required");
  httpd_resp_set_hdr(*this, "WWW-Authenticate", auth_val.c_str());
  httpd_resp_send_err(*this, HTTPD_401_UNAUTHORIZED, nullptr);
}

AsyncWebParameter *AsyncWebServerRequest::getParam(const std::string &name) {
  auto find = this->params_.find(name);
  if (find != this->params_.end()) {
    return find->second;
  }

  optional<std::string> val = query_key_value(this->post_query_, name);
  if (!val.has_value()) {
    auto url_query = request_get_url_query(*this);
    if (url_query.has_value()) {
      val = query_key_value(url_query.value(), name);
    }
  }

  AsyncWebParameter *param = nullptr;
  if (val.has_value()) {
    param = new AsyncWebParameter(val.value());  // NOLINT(cppcoreguidelines-owning-memory)
  }
  this->params_.insert({name, param});
  return param;
}

void AsyncWebServerResponse::addHeader(const char *name, const char *value) {
  httpd_resp_set_hdr(*this->req_, name, value);
}

void AsyncResponseStream::print(float value) { this->print(to_string(value)); }

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
AsyncEventSource::~AsyncEventSource() {
  for (auto *ses : this->sessions_) {
    delete ses;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

void AsyncEventSource::handleRequest(AsyncWebServerRequest *request) {
  auto *rsp =  // NOLINT(cppcoreguidelines-owning-memory)
      new AsyncEventSourceResponse(request, this, this->web_server_);
  if (this->on_connect_) {
    this->on_connect_(rsp);
  }
  this->sessions_.insert(rsp);
}

void AsyncEventSource::loop() {
  // Clean up dead sessions safely
  // This follows the ESP-IDF pattern where free_ctx marks resources as dead
  // and the main loop handles the actual cleanup to avoid race conditions
  auto it = this->sessions_.begin();
  while (it != this->sessions_.end()) {
    auto *ses = *it;
    // If the session has a dead socket (marked by destroy callback)
    if (ses->fd_.load() == 0) {
      ESP_LOGD(TAG, "Removing dead event source session");
      it = this->sessions_.erase(it);
      delete ses;  // NOLINT(cppcoreguidelines-owning-memory)
    } else {
      ses->loop();
      ++it;
    }
  }
}

void AsyncEventSource::try_send_nodefer(const char *message, const char *event, uint32_t id, uint32_t reconnect) {
  for (auto *ses : this->sessions_) {
    if (ses->fd_.load() != 0) {  // Skip dead sessions
      ses->try_send_nodefer(message, event, id, reconnect);
    }
  }
}

void AsyncEventSource::deferrable_send_state(void *source, const char *event_type,
                                             message_generator_t *message_generator) {
  for (auto *ses : this->sessions_) {
    if (ses->fd_.load() != 0) {  // Skip dead sessions
      ses->deferrable_send_state(source, event_type, message_generator);
    }
  }
}

AsyncEventSourceResponse::AsyncEventSourceResponse(const AsyncWebServerRequest *request,
                                                   esphome::web_server_idf::AsyncEventSource *server,
                                                   esphome::web_server::WebServer *ws)
    : server_(server), web_server_(ws), entities_iterator_(new esphome::web_server::ListEntitiesIterator(ws, server)) {
  httpd_req_t *req = *request;

  httpd_resp_set_status(req, HTTPD_200);
  httpd_resp_set_type(req, "text/event-stream");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");

  for (const auto &pair : DefaultHeaders::Instance().headers_) {
    httpd_resp_set_hdr(req, pair.first.c_str(), pair.second.c_str());
  }

  httpd_resp_send_chunk(req, CRLF_STR, CRLF_LEN);

  req->sess_ctx = this;
  req->free_ctx = AsyncEventSourceResponse::destroy;

  this->hd_ = req->handle;
  this->fd_.store(httpd_req_to_sockfd(req));

  // Configure reconnect timeout and send config
  // this should always go through since the tcp send buffer is empty on connect
  std::string message = ws->get_config_json();
  this->try_send_nodefer(message.c_str(), "ping", millis(), 30000);

#ifdef USE_WEBSERVER_SORTING
  for (auto &group : ws->sorting_groups_) {
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
    message = json::build_json([group](JsonObject root) {
      root["name"] = group.second.name;
      root["sorting_weight"] = group.second.weight;
    });
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    // a (very) large number of these should be able to be queued initially without defer
    // since the only thing in the send buffer at this point is the initial ping/config
    this->try_send_nodefer(message.c_str(), "sorting_group");
  }
#endif

  this->entities_iterator_->begin(ws->include_internal_);

  // just dump them all up-front and take advantage of the deferred queue
  //     on second thought that takes too long, but leaving the commented code here for debug purposes
  // while(!this->entities_iterator_->completed()) {
  //  this->entities_iterator_->advance();
  //}
}

void AsyncEventSourceResponse::destroy(void *ptr) {
  auto *rsp = static_cast<AsyncEventSourceResponse *>(ptr);
  ESP_LOGD(TAG, "Event source connection closed (fd: %d)", rsp->fd_.load());
  // Mark as dead by setting fd to 0 - will be cleaned up in the main loop
  rsp->fd_.store(0);
  // Note: We don't delete or remove from set here to avoid race conditions
}

// helper for allowing only unique entries in the queue
void AsyncEventSourceResponse::deq_push_back_with_dedup_(void *source, message_generator_t *message_generator) {
  DeferredEvent item(source, message_generator);

  auto iter = std::find_if(this->deferred_queue_.begin(), this->deferred_queue_.end(),
                           [&item](const DeferredEvent &test) -> bool { return test == item; });

  if (iter != this->deferred_queue_.end()) {
    (*iter) = item;
  } else {
    this->deferred_queue_.push_back(item);
  }
}

void AsyncEventSourceResponse::process_deferred_queue_() {
  while (!deferred_queue_.empty()) {
    DeferredEvent &de = deferred_queue_.front();
    std::string message = de.message_generator_(web_server_, de.source_);
    if (this->try_send_nodefer(message.c_str(), "state")) {
      // O(n) but memory efficiency is more important than speed here which is why std::vector was chosen
      deferred_queue_.erase(deferred_queue_.begin());
    } else {
      break;
    }
  }
}

void AsyncEventSourceResponse::process_buffer_() {
  if (event_buffer_.empty()) {
    return;
  }
  if (event_bytes_sent_ == event_buffer_.size()) {
    event_buffer_.resize(0);
    event_bytes_sent_ = 0;
    return;
  }

  int bytes_sent = httpd_socket_send(this->hd_, this->fd_.load(), event_buffer_.c_str() + event_bytes_sent_,
                                     event_buffer_.size() - event_bytes_sent_, 0);
  if (bytes_sent == HTTPD_SOCK_ERR_TIMEOUT || bytes_sent == HTTPD_SOCK_ERR_FAIL) {
    // Socket error - just return, the connection will be closed by httpd
    // and our destroy callback will be called
    return;
  }
  event_bytes_sent_ += bytes_sent;

  if (event_bytes_sent_ == event_buffer_.size()) {
    event_buffer_.resize(0);
    event_bytes_sent_ = 0;
  }
}

void AsyncEventSourceResponse::loop() {
  process_buffer_();
  process_deferred_queue_();
  if (!this->entities_iterator_->completed())
    this->entities_iterator_->advance();
}

bool AsyncEventSourceResponse::try_send_nodefer(const char *message, const char *event, uint32_t id,
                                                uint32_t reconnect) {
  if (this->fd_.load() == 0) {
    return false;
  }

  process_buffer_();
  if (!event_buffer_.empty()) {
    // there is still pending event data to send first
    return false;
  }

  // 8 spaces are standing in for the hexidecimal chunk length to print later
  const char chunk_len_header[] = "        " CRLF_STR;
  const int chunk_len_header_len = sizeof(chunk_len_header) - 1;

  event_buffer_.append(chunk_len_header);

  if (reconnect) {
    event_buffer_.append("retry: ", sizeof("retry: ") - 1);
    event_buffer_.append(to_string(reconnect));
    event_buffer_.append(CRLF_STR, CRLF_LEN);
  }

  if (id) {
    event_buffer_.append("id: ", sizeof("id: ") - 1);
    event_buffer_.append(to_string(id));
    event_buffer_.append(CRLF_STR, CRLF_LEN);
  }

  if (event && *event) {
    event_buffer_.append("event: ", sizeof("event: ") - 1);
    event_buffer_.append(event);
    event_buffer_.append(CRLF_STR, CRLF_LEN);
  }

  if (message && *message) {
    event_buffer_.append("data: ", sizeof("data: ") - 1);
    event_buffer_.append(message);
    event_buffer_.append(CRLF_STR, CRLF_LEN);
  }

  if (event_buffer_.empty()) {
    return true;
  }

  event_buffer_.append(CRLF_STR, CRLF_LEN);
  event_buffer_.append(CRLF_STR, CRLF_LEN);

  // chunk length header itself and the final chunk terminating CRLF are not counted as part of the chunk
  int chunk_len = event_buffer_.size() - CRLF_LEN - chunk_len_header_len;
  char chunk_len_str[9];
  snprintf(chunk_len_str, 9, "%08x", chunk_len);
  std::memcpy(&event_buffer_[0], chunk_len_str, 8);

  event_bytes_sent_ = 0;
  process_buffer_();

  return true;
}

void AsyncEventSourceResponse::deferrable_send_state(void *source, const char *event_type,
                                                     message_generator_t *message_generator) {
  // allow all json "details_all" to go through before publishing bare state events, this avoids unnamed entries showing
  // up in the web GUI and reduces event load during initial connect
  if (!entities_iterator_->completed() && 0 != strcmp(event_type, "state_detail_all"))
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

  process_buffer_();
  process_deferred_queue_();

  if (!event_buffer_.empty() || !deferred_queue_.empty()) {
    // outgoing event buffer or deferred queue still not empty which means downstream tcp send buffer full, no point
    // trying to send first
    deq_push_back_with_dedup_(source, message_generator);
  } else {
    std::string message = message_generator(web_server_, source);
    if (!this->try_send_nodefer(message.c_str(), "state")) {
      deq_push_back_with_dedup_(source, message_generator);
    }
  }
}
#endif

#ifdef USE_WEBSERVER_OTA
esp_err_t AsyncWebServer::handle_multipart_upload_(httpd_req_t *r, const char *content_type) {
  static constexpr size_t MULTIPART_CHUNK_SIZE = 1460;       // Match Arduino AsyncWebServer buffer size
  static constexpr size_t YIELD_INTERVAL_BYTES = 16 * 1024;  // Yield every 16KB to prevent watchdog

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

  // Process data
  std::unique_ptr<char[]> buffer(new char[MULTIPART_CHUNK_SIZE]);
  size_t bytes_since_yield = 0;

  for (size_t remaining = r->content_len; remaining > 0;) {
    int recv_len = httpd_req_recv(r, buffer.get(), std::min(remaining, MULTIPART_CHUNK_SIZE));

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

}  // namespace web_server_idf
}  // namespace esphome

#endif  // !defined(USE_ESP_IDF)

#ifdef USE_ESP_IDF

#include <cstdarg>

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "esp_tls_crypto.h"

#include "web_server_idf.h"

namespace esphome {
namespace web_server_idf {

#ifndef HTTPD_409
#define HTTPD_409 "409 Conflict"
#endif

#define CRLF_STR "\r\n"
#define CRLF_LEN (sizeof(CRLF_STR) - 1)

static const char *const TAG = "web_server_idf";

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
        .handler = AsyncWebServer::request_handler,
        .user_ctx = this,
    };
    httpd_register_uri_handler(this->server_, &handler_post);
  }
}

esp_err_t AsyncWebServer::request_handler(httpd_req_t *r) {
  ESP_LOGV(TAG, "Enter AsyncWebServer::request_handler. method=%u, uri=%s", r->method, r->uri);
  AsyncWebServerRequest req(r);
  auto *server = static_cast<AsyncWebServer *>(r->user_ctx);
  for (auto *handler : server->handlers_) {
    if (handler->canHandle(&req)) {
      // At now process only basic requests.
      // OTA requires multipart request support and handleUpload for it
      handler->handleRequest(&req);
      return ESP_OK;
    }
  }
  if (server->on_not_found_) {
    server->on_not_found_(&req);
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

optional<std::string> AsyncWebServerRequest::get_header(const char *name) const {
  size_t buf_len = httpd_req_get_hdr_value_len(*this, name);
  if (buf_len == 0) {
    return {};
  }
  auto buf = std::unique_ptr<char[]>(new char[++buf_len]);
  if (!buf) {
    ESP_LOGE(TAG, "No enough memory for get header %s", name);
    return {};
  }
  if (httpd_req_get_hdr_value_str(*this, name, buf.get(), buf_len) != ESP_OK) {
    return {};
  }
  return {buf.get()};
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

static std::string url_decode(const std::string &in) {
  std::string out;
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%') {
      ++i;
      if (i + 1 < in.size()) {
        auto c = parse_hex<uint8_t>(&in[i], 2);
        if (c.has_value()) {
          out += static_cast<char>(*c);
          ++i;
        } else {
          out += '%';
          out += in[i++];
          out += in[i];
        }
      } else {
        out += '%';
        out += in[i];
      }
    } else if (in[i] == '+') {
      out += ' ';
    } else {
      out += in[i];
    }
  }
  return out;
}

AsyncWebParameter *AsyncWebServerRequest::getParam(const std::string &name) {
  auto find = this->params_.find(name);
  if (find != this->params_.end()) {
    return find->second;
  }

  auto query_len = httpd_req_get_url_query_len(this->req_);
  if (query_len == 0) {
    return nullptr;
  }

  auto query_str = std::unique_ptr<char[]>(new char[++query_len]);
  if (!query_str) {
    ESP_LOGE(TAG, "No enough memory for get query param");
    return nullptr;
  }

  auto res = httpd_req_get_url_query_str(*this, query_str.get(), query_len);
  if (res != ESP_OK) {
    ESP_LOGW(TAG, "Can't get query for request: %s", esp_err_to_name(res));
    return nullptr;
  }

  auto query_val = std::unique_ptr<char[]>(new char[query_len]);
  if (!query_val) {
    ESP_LOGE(TAG, "No enough memory for get query param value");
    return nullptr;
  }

  res = httpd_query_key_value(query_str.get(), name.c_str(), query_val.get(), query_len);
  if (res != ESP_OK) {
    this->params_.insert({name, nullptr});
    return nullptr;
  }
  query_str.release();
  auto decoded = url_decode(query_val.get());
  query_val.release();
  auto *param = new AsyncWebParameter(decoded);  // NOLINT(cppcoreguidelines-owning-memory)
  this->params_.insert(std::make_pair(name, param));
  return param;
}

void AsyncWebServerResponse::addHeader(const char *name, const char *value) {
  httpd_resp_set_hdr(*this->req_, name, value);
}

void AsyncResponseStream::print(float value) { this->print(to_string(value)); }

void AsyncResponseStream::printf(const char *fmt, ...) {
  std::string str;
  va_list args;

  va_start(args, fmt);
  size_t length = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);

  str.resize(length);
  va_start(args, fmt);
  vsnprintf(&str[0], length + 1, fmt, args);
  va_end(args);

  this->print(str);
}

AsyncEventSource::~AsyncEventSource() {
  for (auto *ses : this->sessions_) {
    delete ses;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

void AsyncEventSource::handleRequest(AsyncWebServerRequest *request) {
  auto *rsp = new AsyncEventSourceResponse(request, this);  // NOLINT(cppcoreguidelines-owning-memory)
  if (this->on_connect_) {
    this->on_connect_(rsp);
  }
  this->sessions_.insert(rsp);
}

void AsyncEventSource::send(const char *message, const char *event, uint32_t id, uint32_t reconnect) {
  for (auto *ses : this->sessions_) {
    ses->send(message, event, id, reconnect);
  }
}

AsyncEventSourceResponse::AsyncEventSourceResponse(const AsyncWebServerRequest *request, AsyncEventSource *server)
    : server_(server) {
  httpd_req_t *req = *request;

  httpd_resp_set_status(req, HTTPD_200);
  httpd_resp_set_type(req, "text/event-stream");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");

  httpd_resp_send_chunk(req, CRLF_STR, CRLF_LEN);

  req->sess_ctx = this;
  req->free_ctx = AsyncEventSourceResponse::destroy;

  this->hd_ = req->handle;
  this->fd_ = httpd_req_to_sockfd(req);
}

void AsyncEventSourceResponse::destroy(void *ptr) {
  auto *rsp = static_cast<AsyncEventSourceResponse *>(ptr);
  rsp->server_->sessions_.erase(rsp);
  delete rsp;  // NOLINT(cppcoreguidelines-owning-memory)
}

void AsyncEventSourceResponse::send(const char *message, const char *event, uint32_t id, uint32_t reconnect) {
  if (this->fd_ == 0) {
    return;
  }

  std::string ev;

  if (reconnect) {
    ev.append("retry: ", sizeof("retry: ") - 1);
    ev.append(to_string(reconnect));
    ev.append(CRLF_STR, CRLF_LEN);
  }

  if (id) {
    ev.append("id: ", sizeof("id: ") - 1);
    ev.append(to_string(id));
    ev.append(CRLF_STR, CRLF_LEN);
  }

  if (event && *event) {
    ev.append("event: ", sizeof("event: ") - 1);
    ev.append(event);
    ev.append(CRLF_STR, CRLF_LEN);
  }

  if (message && *message) {
    ev.append("data: ", sizeof("data: ") - 1);
    ev.append(message);
    ev.append(CRLF_STR, CRLF_LEN);
  }

  if (ev.empty()) {
    return;
  }

  ev.append(CRLF_STR, CRLF_LEN);

  // Sending chunked content prelude
  auto cs = str_snprintf("%x" CRLF_STR, 4 * sizeof(ev.size()) + CRLF_LEN, ev.size());
  httpd_socket_send(this->hd_, this->fd_, cs.c_str(), cs.size(), 0);

  // Sendiing content chunk
  httpd_socket_send(this->hd_, this->fd_, ev.c_str(), ev.size(), 0);

  // Indicate end of chunk
  httpd_socket_send(this->hd_, this->fd_, CRLF_STR, CRLF_LEN, 0);
}

}  // namespace web_server_idf
}  // namespace esphome

#endif  // !defined(USE_ESP_IDF)

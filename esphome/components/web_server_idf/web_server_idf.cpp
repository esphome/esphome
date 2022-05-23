#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "esp_tls_crypto.h"

#include "web_server_idf.h"

namespace esphome {
namespace web_server_idf {

#ifndef HTTPD_409
#define HTTPD_409 "409 Conflict"
#endif

#define HEADER_CONNECTION "Connection"
#define HEADER_CONNECTION_VALUE_KEEP_ALIVE "keep-alive"
#define HEADER_AUTH "Authorization"
#define HEADER_AUTH_VALUE_BASIC_PREFIX "Basic "
#define CRLF "\r\n"
#define EVENT_KEY_RETRY "retry: "
#define EVENT_KEY_ID "id: "
#define EVENT_KEY_EVENT "event: "
#define EVENT_KEY_DATA "data: "

static const char *const TAG = "web_server_idf";

float arduino_string::toFloat() const {
  auto res = parse_number<float>(*this);
  return res.has_value() ? res.value() : NAN;
}

bool uri_match(const char *reference_uri, const char *uri_to_match, size_t match_upto) {
  // always matched
  return true;
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
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.uri_match_fn = uri_match;
  if (httpd_start(&this->server_, &config) == ESP_OK) {
    const httpd_uri_t handler_get = {
        .uri = "",
        .method = HTTP_GET,
        .handler = request_handler_,
        .user_ctx = this,
    };
    httpd_register_uri_handler(this->server_, &handler_get);

    const httpd_uri_t handler_post = {
        .uri = "",
        .method = HTTP_POST,
        .handler = request_handler_,
        .user_ctx = this,
    };
    httpd_register_uri_handler(this->server_, &handler_post);
  }
}

esp_err_t AsyncWebServer::request_handler_(httpd_req_t *r) {
  AsyncWebServerRequest req(r);
  for (auto handler : static_cast<AsyncWebServer *>(r->user_ctx)->handlers_) {
    if (handler->canHandle(&req)) {
      handler->handleRequest(&req);
      break;
    }
  }
  return ESP_OK;
}

AsyncWebServerRequest::~AsyncWebServerRequest() {
  if (this->rsp_) {
    delete this->rsp_;
  }
  for (auto pair : this->params_) {
    delete pair.second;
  }
}

const std::string AsyncWebServerRequest::url() const {
  auto str = strchr(this->req_->uri, '?');
  if (str == nullptr) {
    return this->req_->uri;
  }
  return std::string(this->req_->uri, str - this->req_->uri);
}

void AsyncWebServerRequest::send(AsyncResponse *response) {
  httpd_resp_send(*this, response->get_content_data(), response->get_content_size());
}

void AsyncWebServerRequest::send(int code, const char *contentType, const char *content) {
  this->init_response_(nullptr, code, contentType);
  if (content) {
    httpd_resp_send(*this, content, HTTPD_RESP_USE_STRLEN);
  } else {
    httpd_resp_send(*this, nullptr, 0);
  }
}

void AsyncWebServerRequest::init_response_(AsyncResponse *rsp, int code, const char *contentType) {
  httpd_resp_set_status(*this, code == 200   ? HTTPD_200
                               : code == 404 ? HTTPD_404
                               : code == 409 ? HTTPD_409
                                             : to_string(code).c_str());

  if (contentType && *contentType) {
    httpd_resp_set_type(*this, contentType);
  }
  httpd_resp_set_hdr(*this, "Accept-Ranges", "none");

  if (this->rsp_) {
    delete this->rsp_;
  }
  this->rsp_ = rsp;
}

bool AsyncWebServerRequest::authenticate(const char *username, const char *password, const char *realm,
                                         bool passwordIsHash) const {
  if (username == nullptr || password == nullptr || *username == 0) {
    return true;
  }
  size_t buf_len = httpd_req_get_hdr_value_len(*this, HEADER_AUTH);
  if (buf_len == 0) {
    return false;
  }
  char *buf = new char[++buf_len];
  if (buf == nullptr) {
    ESP_LOGE(TAG, "No enough memory for basic authorization");
    return false;
  }
  if (httpd_req_get_hdr_value_str(*this, HEADER_AUTH, buf, buf_len) != ESP_OK) {
    delete[] buf;
    return false;
  }
  const size_t auth_prefix_len = sizeof(HEADER_AUTH_VALUE_BASIC_PREFIX) - 1;
  if (strncmp(HEADER_AUTH_VALUE_BASIC_PREFIX, buf, auth_prefix_len) != 0) {
    ESP_LOGW(TAG, "Only Basic authorization supported yet");
    delete[] buf;
    return false;
  }

  std::string user_info;
  user_info += username;
  user_info += ':';
  user_info += password;

  size_t n = 0, out;
  esp_crypto_base64_encode(nullptr, 0, &n, reinterpret_cast<const uint8_t *>(user_info.c_str()), user_info.size());

  uint8_t *digest = new uint8_t[n + 1];
  esp_crypto_base64_encode(digest, n, &out, reinterpret_cast<const uint8_t *>(user_info.c_str()), user_info.size());

  bool res = strncmp(reinterpret_cast<const char *>(digest), buf + auth_prefix_len, buf_len - auth_prefix_len) == 0;

  delete[] buf;
  delete[] digest;

  return res;
}

void AsyncWebServerRequest::requestAuthentication(const char *realm, bool isDigest) const {
  httpd_resp_set_hdr(*this, HEADER_CONNECTION, HEADER_CONNECTION_VALUE_KEEP_ALIVE);
  // TODO implement digest support ???
  auto auth_val = str_sprintf(HEADER_AUTH_VALUE_BASIC_PREFIX "realm=\"%s\"", realm ? realm : "Login Required");
  httpd_resp_set_hdr(*this, "WWW-Authenticate", auth_val.c_str());
  httpd_resp_send_err(*this, HTTPD_401_UNAUTHORIZED, nullptr);
}

bool AsyncWebServerRequest::hasParam(const std::string &name, bool post, bool file) {
  return this->getParam(name, post, file) != nullptr;
}

AsyncWebParameter *AsyncWebServerRequest::getParam(const std::string &name, bool post, bool file) {
  auto find = this->params_.find(name);
  if (find != this->params_.end()) {
    return find->second;
  }

  auto query_len = httpd_req_get_url_query_len(this->req_);
  if (query_len == 0) {
    return nullptr;
  }

  auto query_str = new char[++query_len];
  if (query_str == nullptr) {
    ESP_LOGE(TAG, "No enough memory for get query param");
    return nullptr;
  }

  auto res = httpd_req_get_url_query_str(*this, query_str, query_len);
  if (res != ESP_OK) {
    ESP_LOGW(TAG, "Can't get query for request: %s", esp_err_to_name(res));
    delete[] query_str;
    return nullptr;
  }

  auto query_val = new char[query_len];
  if (query_val == nullptr) {
    ESP_LOGE(TAG, "No enough memory for get query param value");
    delete[] query_str;
    return nullptr;
  }

  res = httpd_query_key_value(query_str, name.c_str(), query_val, query_len);
  if (res != ESP_OK) {
    delete[] query_val;
    delete[] query_str;
    this->params_.insert({name, nullptr});
    return nullptr;
  }
  delete[] query_str;

  auto param = new AsyncWebParameter(name, query_val, post, file);
  delete[] query_val;

  this->params_.insert({name, param});
  return param;
}

void AsyncResponse::addHeader(const char *name, const char *value) { httpd_resp_set_hdr(*this->req_, name, value); }

void AsyncResponseStream::print(float value) { this->print(to_string(value)); }

AsyncEventSource::~AsyncEventSource() {
  for (auto ses : this->sessions_) {
    delete ses;
  }
}

void AsyncEventSource::handleRequest(AsyncWebServerRequest *request) {
  auto rsp = new AsyncEventSourceResponse(request, this);
  if (this->on_connect_) {
    this->on_connect_(rsp);
  }
  this->sessions_.insert(rsp);
}

void AsyncEventSource::send(const char *message, const char *event, uint32_t id, uint32_t reconnect) {
  for (auto ses : this->sessions_) {
    ses->send(message, event, id, reconnect);
  }
}

AsyncEventSourceResponse::AsyncEventSourceResponse(const AsyncWebServerRequest *request, AsyncEventSource *server)
    : server_(server) {
  httpd_req_t *req = *request;

  httpd_resp_set_status(req, HTTPD_200);
  httpd_resp_set_type(req, "text/event-stream");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, HEADER_CONNECTION, HEADER_CONNECTION_VALUE_KEEP_ALIVE);

  httpd_resp_send_chunk(req, CRLF, sizeof(CRLF) - 1);

  req->sess_ctx = this;
  req->free_ctx = AsyncEventSourceResponse::destroy;

  this->hd_ = req->handle;
  this->fd_ = httpd_req_to_sockfd(req);
}

void AsyncEventSourceResponse::destroy(void *ptr) {
  auto rsp = static_cast<AsyncEventSourceResponse *>(ptr);
  rsp->server_->sessions_.erase(rsp);
  delete rsp;
}

void AsyncEventSourceResponse::send(const char *message, const char *event, uint32_t id, uint32_t reconnect) {
  if (this->fd_ == 0) {
    return;
  }

  std::string ev;

  if (reconnect) {
    ev.append(EVENT_KEY_RETRY, sizeof(EVENT_KEY_RETRY) - 1);
    ev.append(to_string(reconnect));
    ev.append(CRLF, sizeof(CRLF) - 1);
  }

  if (id) {
    ev.append(EVENT_KEY_ID, sizeof(EVENT_KEY_ID) - 1);
    ev.append(to_string(id));
    ev.append(CRLF, sizeof(CRLF) - 1);
  }

  if (event && *event) {
    ev.append(EVENT_KEY_EVENT, sizeof(EVENT_KEY_EVENT) - 1);
    ev.append(event);
    ev.append(CRLF, sizeof(CRLF) - 1);
  }

  if (message && *message) {
    ev.append(EVENT_KEY_DATA, sizeof(EVENT_KEY_DATA) - 1);
    ev.append(message);
    ev.append(CRLF, sizeof(CRLF) - 1);
  }

  if (ev.empty()) {
    return;
  }

  ev.append(CRLF, sizeof(CRLF) - 1);

  // Sending chunked content prelude
  auto cs = str_snprintf("%x" CRLF, 4 * sizeof(ev.size()) + sizeof(CRLF) - 1, ev.size());
  httpd_socket_send(this->hd_, this->fd_, cs.c_str(), cs.size(), 0);

  // Sendiing content chunk
  httpd_socket_send(this->hd_, this->fd_, ev.c_str(), ev.size(), 0);

  // Indicate end of chunk
  httpd_socket_send(this->hd_, this->fd_, CRLF, sizeof(CRLF) - 1, 0);
}

}  // namespace web_server_idf
}  // namespace esphome

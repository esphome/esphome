#ifdef USE_ESP_IDF

#include <cstdarg>

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "esp_tls_crypto.h"

#include "web_server_idf.h"

namespace esphome {
namespace web_server_idf {

#define USE_WEBSERVER_IDF_MULTIPART

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
#ifdef USE_WEBSERVER_IDF_MULTIPART
class FormDataParser {
  void debug_(const char *name, const char *s, int len = -1) {}

 protected:
  enum {
    PST_BOUNDARY = 0,
    PST_HEADERS,
    PST_DATA,
    PST_FINISHED,
  } state_ = PST_BOUNDARY;
  std::string boundary_end_;
  std::string filename_;
  char *buf_{};
  int buf_start_{};
  char *data_{};
  AsyncWebHandler *handler_{};

  bool find_filename_(char *str, size_t len) {
    constexpr auto c_cd_str = "Content-Disposition: form-data;";
    constexpr auto c_cd_len = sizeof("Content-Disposition: form-data;") - 1;
    if (strncasecmp(str, c_cd_str, c_cd_len) != 0) {
      return false;
    }
    str += c_cd_len;
    len -= c_cd_len;

    constexpr auto c_fn_str = "filename=\"";
    constexpr auto c_fn_len = sizeof("filename=\"") - 1;
    str = static_cast<char *>(memmem(str, strnlen(str, len), c_fn_str, c_fn_len));
    if (str == nullptr) {
      return false;
    }
    str += c_fn_len;
    len -= c_fn_len;

    auto *end = static_cast<char *>(memchr(str, '"', strnlen(str, len)));
    if (end == nullptr) {
      return false;
    }

    this->filename_ = std::string(str, end);
    ESP_LOGVV(TAG, "found filename %s", this->filename_.c_str());
    return true;
  }

  // return -1 - fail, 1 - continue reading, 0 - start of data
  int process_headers_(int &received, int prev_buf_start) {
    char *header = this->data_;
    debug_("header start", header);
    char *header_end = static_cast<char *>(memmem(header, strnlen(header, received), CRLF_STR, CRLF_LEN));
    if (header_end == nullptr) {
      if (prev_buf_start > 0) {
        // header is longer than BUF_SIZE*2
        this->data_ += received;
        // debug_("fail header", header, header_end - header);
        return 0;
      } else {
        this->buf_start_ = received - (header - this->buf_);
        return 0;
      }
    }

    debug_("header", header, header_end - header);

    if (this->filename_.empty()) {
      this->find_filename_(header, header_end - header);
    }

    header_end += CRLF_LEN;
    debug_("header end", header_end);
    // search additional CRLF_STR
    size_t tail_len = received - (header_end - this->buf_);
    if (tail_len >= CRLF_LEN && *header_end == CRLF_STR[0] && *(++header_end) == CRLF_STR[1]) {
      ++header_end;
      ESP_LOGVV(TAG, "received %d\n", received);
      received -= header_end - this->buf_;
      ESP_LOGVV(TAG, "received %d\n", received);
      this->data_ = header_end;
      this->state_ = PST_DATA;
      return 0;
    }

    this->data_ = header_end;
    return 1;
  }

  // returns 0 - no error, -1 - failed, 1 - no boundary
  int find_boundary_(const AsyncWebServerRequest &req) {
    auto content_type = req.get_header("Content-Type");
    constexpr auto c_multipart_form_data_str = "multipart/form-data";
    constexpr auto c_multipart_form_data_len = sizeof("multipart/form-data") - 1;
    if (!content_type.has_value() || !str_startswith(*content_type, c_multipart_form_data_str)) {
      return 1;
    }
    const char *boundary = content_type.value().c_str() + (c_multipart_form_data_len + 1);  // also skips ';'
    while (*boundary == ' ') {
      boundary++;
    }
    constexpr auto c_boundary_str = "boundary=";
    constexpr auto c_boundary_len = sizeof("boundary=") - 1;
    if (strncmp(boundary, c_boundary_str, c_boundary_len) != 0) {
      return -1;
    }
    boundary += c_boundary_len;
    size_t boundary_len = content_type.value().length() - (boundary - content_type.value().c_str());

    this->boundary_end_ = "\r\n--";
    this->boundary_end_ += std::string(boundary, boundary_len);
    ESP_LOGVV(TAG, "boundary %s", this->boundary_end_.c_str() + CRLF_LEN);
    return 0;
  }

  // finding start of boundary "--%s\r\n"
  int process_boundary_() {
    const auto *boundary_start = this->boundary_end_.c_str() + CRLF_LEN;
    const auto boundary_size = this->boundary_end_.size() - CRLF_LEN;
    if (strncmp(this->data_, boundary_start, boundary_size) != 0) {
      return -1;
    }
    this->data_ += boundary_size;
    if (strncmp(this->data_, CRLF_STR, CRLF_LEN) != 0) {
      return -1;
    }
    this->data_ += CRLF_LEN;

    this->state_ = PST_HEADERS;
    return 0;
  }

  int process_data_tail_(int received) {
    auto *tail = this->data_ + received - 1;
    // check tail \r
    if (*tail == '\r') {
      this->data_ = tail;
      this->buf_start_ = 1;
      return received - 1;
    }
    // check tail \r\n
    if (*tail == '\n' && *(--tail) == '\r') {
      this->data_ = tail;
      this->buf_start_ = 2;
      return received - 2;
    }
    // check tail \r\n-
    if (*tail == '-' && *(--tail) == '\n' && *(--tail) == '\r') {
      this->data_ = tail;
      this->buf_start_ = 3;
      return received - 3;
    }
    // check tail \r\n-- (tail already point to -2)
    if (*tail == '-' && *(--tail) == '\n' && *(--tail) == '\r') {
      this->data_ = tail;
      this->buf_start_ = 4;
      return received - 4;
    }
    return received;
  }

  // returns 1 - error, 0 - to continue reading, N - number of bytes readed
  int process_data_(int received, int prev_buf_start) {
    char *data_end = static_cast<char *>(memmem(this->data_, received, "\r\n--", sizeof("\r\n--") - 1));
    debug_("data end", data_end);
    if (data_end == nullptr) {
      return this->process_data_tail_(received);
    }

    size_t tail_len = received - (data_end - this->buf_);
    ESP_LOGVV(TAG, "tail_len=%d\n", tail_len);
    if (tail_len < this->boundary_end_.size()) {
      if (prev_buf_start > 0) {
        return -1;
      }
      auto res = received - tail_len;
      this->data_ = this->buf_ + res;
      this->buf_start_ = tail_len;
      return res;
    }

    debug_("data end", data_end);

    data_end = static_cast<char *>(memmem(data_end, tail_len, this->boundary_end_.c_str(), this->boundary_end_.size()));
    if (data_end) {
      this->state_ = PST_FINISHED;
      return (data_end - this->buf_);
    }

    return this->process_data_tail_(received);
  }

 public:
  FormDataParser(AsyncWebHandler *handler) : handler_(handler) {}
  ~FormDataParser() { delete[] this->buf_; }

  // returns 0 - ok and handled, <0 - error, >0 - no form multipart data
  int parse(AsyncWebServerRequest &req) {
    if (req.method() != HTTP_POST) {
      return 1;
    }
    auto res = this->find_boundary_(req);
    if (res < 0) {
      ESP_LOGW(TAG, "Failed find form data boundary attribute");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, nullptr);
      return res;
    }
    if (res > 0) {
      return res;
    }

    int remaining = req.contentLength();
    int index{};

    delete[] this->buf_;
    constexpr auto buf_size = 128;
    // buf_size * 2 - may used in process headers and data
    this->buf_ = new char[(buf_size * 2) + this->boundary_end_.size() + 1];  // NOLINT, 1 - null term

    while (remaining > 0) {
      ESP_LOGD(TAG, "Remaining size: %u", remaining);
      int received = httpd_req_recv(req, this->buf_ + this->buf_start_, std::min(remaining, buf_size));
      if (received <= 0) {
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
          // Retry if timeout occurred
          continue;
        }
        ESP_LOGW(TAG, "File reception failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, nullptr);
        return ESP_FAIL;
      }
      remaining -= received;

      received += this->buf_start_;

      if (this->state_ == PST_FINISHED) {
        debug_("processing PST_FINISHED", std::string(this->data_, received).c_str());
        // file uploaded, do read until stream end
        continue;
      }

      auto prev_buf_start = this->buf_start_;
      this->buf_start_ = 0;

      this->data_ = this->buf_;
      this->data_[received] = 0;  // DEBUG

      if (this->state_ == PST_BOUNDARY) {
        debug_("processing PST_BOUNDARY", this->data_);

        res = this->process_boundary_();
        if (res < 0) {
          ESP_LOGW(TAG, "Failed find multipart form data boundary");
          httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, nullptr);
          return ESP_FAIL;
        }
        // if (err > 0) {
        //   continue;
        // }
      }

      if (this->state_ == PST_HEADERS) {
        debug_("processing PST_HEADERS", this->data_);
        do {
          res = this->process_headers_(received, prev_buf_start);
          if (res < 0) {
            ESP_LOGW(TAG, "Failed parse multipart form data header");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, nullptr);
            return ESP_FAIL;
          }
        } while (res != 0);
      }

      if (this->state_ == PST_DATA) {
        debug_("processing PST_DATA", this->data_);
        res = this->process_data_(received, prev_buf_start);
        if (res < 0) {
          ESP_LOGW(TAG, "Failed parse multipart form data binary");
          httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, nullptr);
          return ESP_FAIL;
        }
        if (res > 0 || this->state_ == PST_FINISHED) {
          this->handler_->handleUpload(&req, this->filename_, index++, reinterpret_cast<uint8_t *>(this->buf_), res,
                                       this->state_ == PST_FINISHED);
        }
      }

      if (this->buf_start_) {
        ESP_LOGVV(TAG, "buf_start_=%d\n", buf_start_);
        std::memcpy(this->buf_, this->data_, this->buf_start_);
      }

      ESP_LOGVV(TAG, "remaining=%d, received=%d", remaining, received);
    }

    return ESP_OK;
  }
};
#endif  // !defined(USE_WEBSERVER_IDF_MULTIPART)

esp_err_t AsyncWebServer::request_handler(httpd_req_t *r) {
  ESP_LOGV(TAG, "Enter AsyncWebServer::request_handler. method=%u, uri=%s", r->method, r->uri);
  AsyncWebServerRequest req(r);
  auto *server = static_cast<AsyncWebServer *>(r->user_ctx);
  for (auto *handler : server->handlers_) {
    if (handler->canHandle(&req)) {
#ifdef USE_WEBSERVER_IDF_MULTIPART
      auto res = FormDataParser(handler).parse(req);
      if (res < 0) {
        return ESP_FAIL;
      }
      if (res > 0) {
        handler->handleRequest(&req);
      }
#else
      handler->handleRequest(&req);
#endif
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

void AsyncWebServerRequest::send(AsyncResponse *response) {
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

void AsyncWebServerRequest::init_response_(AsyncResponse *rsp, int code, const char *content_type) {
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

void AsyncResponse::addHeader(const char *name, const char *value) { httpd_resp_set_hdr(*this->req_, name, value); }

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

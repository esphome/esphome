
#ifdef USE_ESP_IDF
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#include "web_server_idf.h"

namespace esphome {
namespace web_server_idf {

#define CRLF_STR "\r\n"
#define CRLF_LEN (sizeof(CRLF_STR) - 1)

static const char *const TAG = "web_server_idf_multipart";

#ifdef USE_WEBSERVER_IDF_MULTIPART
class FormDataParser {
  void trace_(const char *name, const char *s) { ESP_LOGVV(TAG, name, s); }

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
    trace_("header start", header);
    char *header_end = static_cast<char *>(memmem(header, strnlen(header, received), CRLF_STR, CRLF_LEN));
    if (header_end == nullptr) {
      if (prev_buf_start > 0) {
        // header is longer than BUF_SIZE*2
        this->data_ += received;
        // trace_("fail header", std::string(header, header_end - header).c_str());
        return 0;
      } else {
        this->buf_start_ = received - (header - this->buf_);
        return 0;
      }
    }

    trace_("header", std::string(header, header_end - header).c_str());

    if (this->filename_.empty()) {
      this->find_filename_(header, header_end - header);
    }

    header_end += CRLF_LEN;
    trace_("header end", header_end);
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
    trace_("data end", data_end);
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

    trace_("data end", data_end);

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
        trace_("processing PST_FINISHED", std::string(this->data_, received).c_str());
        // file uploaded, do read until stream end
        continue;
      }

      auto prev_buf_start = this->buf_start_;
      this->buf_start_ = 0;

      this->data_ = this->buf_;
      this->data_[received] = 0;  // DEBUG

      if (this->state_ == PST_BOUNDARY) {
        trace_("processing PST_BOUNDARY", this->data_);

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
        trace_("processing PST_HEADERS", this->data_);
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
        trace_("processing PST_DATA", this->data_);
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

bool handle_multipart_request(AsyncWebHandler *handler, AsyncWebServerRequest &req) {
  auto res = FormDataParser(handler).parse(req);
  if (res < 0) {
    return false;
  }
  if (res > 0) {
    handler->handleRequest(&req);
  }
  return true;
}
#else
bool handle_multipart_request(AsyncWebHandler *handler, AsyncWebServerRequest &req) {
  handler->handleRequest(&req);
  return true;
}
#endif  // USE_WEBSERVER_IDF_MULTIPART

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_ESP_IDF

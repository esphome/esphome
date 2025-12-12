#ifdef USE_ESP32
#include <memory>
#include <cstring>
#include <cctype>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "http_parser.h"

#include "utils.h"

namespace esphome {
namespace web_server_idf {

static const char *const TAG = "web_server_idf_utils";

void url_decode(char *str) {
  char *ptr = str, buf;
  for (; *str; str++, ptr++) {
    if (*str == '%') {
      str++;
      if (parse_hex(str, 2, reinterpret_cast<uint8_t *>(&buf), 1) == 2) {
        *ptr = buf;
        str++;
      } else {
        str--;
        *ptr = *str;
      }
    } else if (*str == '+') {
      *ptr = ' ';
    } else {
      *ptr = *str;
    }
  }
  *ptr = *str;
}

bool request_has_header(httpd_req_t *req, const char *name) { return httpd_req_get_hdr_value_len(req, name); }

optional<std::string> request_get_header(httpd_req_t *req, const char *name) {
  size_t len = httpd_req_get_hdr_value_len(req, name);
  if (len == 0) {
    return {};
  }

  std::string str;
  str.resize(len);

  auto res = httpd_req_get_hdr_value_str(req, name, &str[0], len + 1);
  if (res != ESP_OK) {
    return {};
  }

  return {str};
}

optional<std::string> request_get_url_query(httpd_req_t *req) {
  auto len = httpd_req_get_url_query_len(req);
  if (len == 0) {
    return {};
  }

  std::string str;
  str.resize(len);

  auto res = httpd_req_get_url_query_str(req, &str[0], len + 1);
  if (res != ESP_OK) {
    ESP_LOGW(TAG, "Can't get query for request: %s", esp_err_to_name(res));
    return {};
  }

  return {str};
}

optional<std::string> query_key_value(const std::string &query_url, const std::string &key) {
  if (query_url.empty()) {
    return {};
  }

  auto val = std::unique_ptr<char[]>(new char[query_url.size()]);
  if (!val) {
    ESP_LOGE(TAG, "Not enough memory to the query key value");
    return {};
  }

  if (httpd_query_key_value(query_url.c_str(), key.c_str(), val.get(), query_url.size()) != ESP_OK) {
    return {};
  }

  url_decode(val.get());
  return {val.get()};
}

// Helper function for case-insensitive string region comparison
bool str_ncmp_ci(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (!char_equals_ci(s1[i], s2[i])) {
      return false;
    }
  }
  return true;
}

// Case-insensitive string search (like strstr but case-insensitive)
const char *stristr(const char *haystack, const char *needle) {
  if (!haystack) {
    return nullptr;
  }

  size_t needle_len = strlen(needle);
  if (needle_len == 0) {
    return haystack;
  }

  for (const char *p = haystack; *p; p++) {
    if (str_ncmp_ci(p, needle, needle_len)) {
      return p;
    }
  }

  return nullptr;
}

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_ESP32

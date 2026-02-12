#ifdef USE_ESP32
#include <cstring>
#include <cctype>
#include "esphome/core/helpers.h"
#include "http_parser.h"

#include "utils.h"

namespace esphome::web_server_idf {

size_t url_decode(char *str) {
  char *start = str;
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
  *ptr = '\0';
  return ptr - start;
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

optional<std::string> query_key_value(const char *query_url, size_t query_len, const char *key) {
  if (query_url == nullptr || query_len == 0) {
    return {};
  }

  char val[CONFIG_HTTPD_MAX_URI_LEN + 1];
  if (httpd_query_key_value(query_url, key, val, query_len) != ESP_OK) {
    return {};
  }

  url_decode(val);
  return {val};
}

bool query_has_key(const char *query_url, size_t query_len, const char *key) {
  if (query_url == nullptr || query_len == 0) {
    return false;
  }
  // Minimal buffer — we only care if the key exists, not the value
  char buf[1];
  // httpd_query_key_value returns ESP_OK if key found (even if buffer too small for value),
  // ESP_ERR_NOT_FOUND if key absent
  return httpd_query_key_value(query_url, key, buf, sizeof(buf)) != ESP_ERR_NOT_FOUND;
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

// Bounded case-insensitive string search (like strcasestr but length-bounded)
const char *strcasestr_n(const char *haystack, size_t haystack_len, const char *needle) {
  if (!haystack) {
    return nullptr;
  }

  size_t needle_len = strlen(needle);
  if (needle_len == 0) {
    return haystack;
  }

  if (haystack_len < needle_len) {
    return nullptr;
  }

  const char *end = haystack + haystack_len - needle_len + 1;
  for (const char *p = haystack; p < end; p++) {
    if (str_ncmp_ci(p, needle, needle_len)) {
      return p;
    }
  }

  return nullptr;
}

}  // namespace esphome::web_server_idf
#endif  // USE_ESP32

#pragma once
#ifdef USE_ESP32

#include <esp_http_server.h>
#include <string>
#include "esphome/core/helpers.h"

namespace esphome {
namespace web_server_idf {

bool request_has_header(httpd_req_t *req, const char *name);
optional<std::string> request_get_header(httpd_req_t *req, const char *name);
optional<std::string> request_get_url_query(httpd_req_t *req);
optional<std::string> query_key_value(const std::string &query_url, const std::string &key);

// Helper function for case-insensitive character comparison
inline bool char_equals_ci(char a, char b) { return ::tolower(a) == ::tolower(b); }

// Helper function for case-insensitive string region comparison
bool str_ncmp_ci(const char *s1, const char *s2, size_t n);

// Case-insensitive string search (like strstr but case-insensitive)
const char *stristr(const char *haystack, const char *needle);

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_ESP32

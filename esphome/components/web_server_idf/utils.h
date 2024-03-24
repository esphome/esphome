#pragma once
#ifdef USE_ESP_IDF

#include <esp_http_server.h>
#include "esphome/core/helpers.h"

namespace esphome {
namespace web_server_idf {

bool request_has_header(httpd_req_t *req, const char *name);
optional<std::string> request_get_header(httpd_req_t *req, const char *name);
optional<std::string> request_get_url_query(httpd_req_t *req);
optional<std::string> query_key_value(const std::string &query_url, const std::string &key);

}  // namespace web_server_idf
}  // namespace esphome
#endif  // USE_ESP_IDF

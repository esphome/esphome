#pragma once
#ifdef USE_ESP_IDF

#include "web_server_idf.h"

namespace esphome {
namespace web_server_idf {

bool handle_multipart_request(AsyncWebHandler *handler, AsyncWebServerRequest &req);

}  // namespace web_server_idf
}  // namespace esphome
#endif

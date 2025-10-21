#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_WEBSERVER_OTA)

#include <cctype>
#include <cstring>
#include <esp_http_server.h>
#include <functional>
#include <multipart_parser.h>
#include <string>
#include <utility>

namespace esphome {
namespace web_server_idf {

// Wrapper around zorxx/multipart-parser for ESP-IDF OTA uploads
class MultipartReader {
 public:
  struct Part {
    std::string name;
    std::string filename;
    std::string content_type;
  };

  // IMPORTANT: The data pointer in DataCallback is only valid during the callback!
  // The multipart parser passes pointers to its internal buffer which will be
  // overwritten after the callback returns. Callbacks MUST process or copy the
  // data immediately - storing the pointer for deferred processing will result
  // in use-after-free bugs.
  using DataCallback = std::function<void(const uint8_t *data, size_t len)>;
  using PartCompleteCallback = std::function<void()>;

  explicit MultipartReader(const std::string &boundary);
  ~MultipartReader();

  // Set callbacks for handling data
  void set_data_callback(DataCallback callback) { data_callback_ = std::move(callback); }
  void set_part_complete_callback(PartCompleteCallback callback) { part_complete_callback_ = std::move(callback); }

  // Parse incoming data
  size_t parse(const char *data, size_t len);

  // Get current part info
  const Part &get_current_part() const { return current_part_; }

  // Check if we found a file upload
  bool has_file() const { return !current_part_.filename.empty(); }

 private:
  static int on_header_field(multipart_parser *parser, const char *at, size_t length);
  static int on_header_value(multipart_parser *parser, const char *at, size_t length);
  static int on_part_data(multipart_parser *parser, const char *at, size_t length);
  static int on_part_data_end(multipart_parser *parser);

  multipart_parser *parser_{nullptr};
  multipart_parser_settings settings_{};

  Part current_part_;
  std::string current_header_field_;

  DataCallback data_callback_;
  PartCompleteCallback part_complete_callback_;

  void process_header_(const char *value, size_t length);
};

// ========== Utility Functions ==========

// Case-insensitive string prefix check
bool str_startswith_case_insensitive(const std::string &str, const std::string &prefix);

// Extract a parameter value from a header line
// Handles both quoted and unquoted values
std::string extract_header_param(const std::string &header, const std::string &param);

// Parse boundary from Content-Type header
// Returns true if boundary found, false otherwise
// boundary_start and boundary_len will point to the boundary value
bool parse_multipart_boundary(const char *content_type, const char **boundary_start, size_t *boundary_len);

// Trim whitespace from both ends of a string
std::string str_trim(const std::string &str);

}  // namespace web_server_idf
}  // namespace esphome
#endif  // defined(USE_ESP32) && defined(USE_WEBSERVER_OTA)

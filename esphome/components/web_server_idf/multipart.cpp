#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_WEBSERVER_OTA)
#include "multipart.h"
#include "utils.h"
#include "esphome/core/log.h"
#include <cstring>
#include "multipart_parser.h"

namespace esphome {
namespace web_server_idf {

static const char *const TAG = "multipart";

// ========== MultipartReader Implementation ==========

MultipartReader::MultipartReader(const std::string &boundary) {
  // Initialize settings with callbacks
  memset(&settings_, 0, sizeof(settings_));
  settings_.on_header_field = on_header_field;
  settings_.on_header_value = on_header_value;
  settings_.on_part_data = on_part_data;
  settings_.on_part_data_end = on_part_data_end;

  ESP_LOGV(TAG, "Initializing multipart parser with boundary: '%s' (len: %zu)", boundary.c_str(), boundary.length());

  // Create parser with boundary
  parser_ = multipart_parser_init(boundary.c_str(), &settings_);
  if (parser_) {
    multipart_parser_set_data(parser_, this);
  } else {
    ESP_LOGE(TAG, "Failed to initialize multipart parser");
  }
}

MultipartReader::~MultipartReader() {
  if (parser_) {
    multipart_parser_free(parser_);
  }
}

size_t MultipartReader::parse(const char *data, size_t len) {
  if (!parser_) {
    ESP_LOGE(TAG, "Parser not initialized");
    return 0;
  }

  size_t parsed = multipart_parser_execute(parser_, data, len);

  if (parsed != len) {
    ESP_LOGW(TAG, "Parser consumed %zu of %zu bytes - possible error", parsed, len);
  }

  return parsed;
}

void MultipartReader::process_header_(const char *value, size_t length) {
  // Process the completed header (field + value pair)
  std::string value_str(value, length);

  if (str_startswith_case_insensitive(current_header_field_, "content-disposition")) {
    // Parse name and filename from Content-Disposition
    current_part_.name = extract_header_param(value_str, "name");
    current_part_.filename = extract_header_param(value_str, "filename");
  } else if (str_startswith_case_insensitive(current_header_field_, "content-type")) {
    current_part_.content_type = str_trim(value_str);
  }

  // Clear field for next header
  current_header_field_.clear();
}

int MultipartReader::on_header_field(multipart_parser *parser, const char *at, size_t length) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));
  reader->current_header_field_.assign(at, length);
  return 0;
}

int MultipartReader::on_header_value(multipart_parser *parser, const char *at, size_t length) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));
  reader->process_header_(at, length);
  return 0;
}

int MultipartReader::on_part_data(multipart_parser *parser, const char *at, size_t length) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));
  // Only process file uploads
  if (reader->has_file() && reader->data_callback_) {
    // IMPORTANT: The 'at' pointer points to data within the parser's input buffer.
    // This data is only valid during this callback. The callback handler MUST
    // process or copy the data immediately - it cannot store the pointer for
    // later use as the buffer will be overwritten.
    reader->data_callback_(reinterpret_cast<const uint8_t *>(at), length);
  }
  return 0;
}

int MultipartReader::on_part_data_end(multipart_parser *parser) {
  MultipartReader *reader = static_cast<MultipartReader *>(multipart_parser_get_data(parser));
  ESP_LOGV(TAG, "Part data end");
  if (reader->part_complete_callback_) {
    reader->part_complete_callback_();
  }
  // Clear part info for next part
  reader->current_part_ = Part{};
  return 0;
}

// ========== Utility Functions ==========

// Case-insensitive string prefix check
bool str_startswith_case_insensitive(const std::string &str, const std::string &prefix) {
  if (str.length() < prefix.length()) {
    return false;
  }
  return str_ncmp_ci(str.c_str(), prefix.c_str(), prefix.length());
}

// Extract a parameter value from a header line
// Handles both quoted and unquoted values
std::string extract_header_param(const std::string &header, const std::string &param) {
  size_t search_pos = 0;

  while (search_pos < header.length()) {
    // Look for param name
    const char *found = stristr(header.c_str() + search_pos, param.c_str());
    if (!found) {
      return "";
    }
    size_t pos = found - header.c_str();

    // Check if this is a word boundary (not part of another parameter)
    if (pos > 0 && header[pos - 1] != ' ' && header[pos - 1] != ';' && header[pos - 1] != '\t') {
      search_pos = pos + 1;
      continue;
    }

    // Move past param name
    pos += param.length();

    // Skip whitespace and find '='
    while (pos < header.length() && (header[pos] == ' ' || header[pos] == '\t')) {
      pos++;
    }

    if (pos >= header.length() || header[pos] != '=') {
      search_pos = pos;
      continue;
    }

    pos++;  // Skip '='

    // Skip whitespace after '='
    while (pos < header.length() && (header[pos] == ' ' || header[pos] == '\t')) {
      pos++;
    }

    if (pos >= header.length()) {
      return "";
    }

    // Check if value is quoted
    if (header[pos] == '"') {
      pos++;
      size_t end = header.find('"', pos);
      if (end != std::string::npos) {
        return header.substr(pos, end - pos);
      }
      // Malformed - no closing quote
      return "";
    }

    // Unquoted value - find the end (semicolon, comma, or end of string)
    size_t end = pos;
    while (end < header.length() && header[end] != ';' && header[end] != ',' && header[end] != ' ' &&
           header[end] != '\t') {
      end++;
    }

    return header.substr(pos, end - pos);
  }

  return "";
}

// Parse boundary from Content-Type header
// Returns true if boundary found, false otherwise
// boundary_start and boundary_len will point to the boundary value
bool parse_multipart_boundary(const char *content_type, const char **boundary_start, size_t *boundary_len) {
  if (!content_type) {
    return false;
  }

  // Check for multipart/form-data (case-insensitive)
  if (!stristr(content_type, "multipart/form-data")) {
    return false;
  }

  // Look for boundary parameter
  const char *b = stristr(content_type, "boundary=");
  if (!b) {
    return false;
  }

  const char *start = b + 9;  // Skip "boundary="

  // Skip whitespace
  while (*start == ' ' || *start == '\t') {
    start++;
  }

  if (!*start) {
    return false;
  }

  // Find end of boundary
  const char *end = start;
  if (*end == '"') {
    // Quoted boundary
    start++;
    end++;
    while (*end && *end != '"') {
      end++;
    }
    *boundary_len = end - start;
  } else {
    // Unquoted boundary
    while (*end && *end != ' ' && *end != ';' && *end != '\r' && *end != '\n' && *end != '\t') {
      end++;
    }
    *boundary_len = end - start;
  }

  if (*boundary_len == 0) {
    return false;
  }

  *boundary_start = start;

  return true;
}

// Trim whitespace from both ends of a string
std::string str_trim(const std::string &str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

}  // namespace web_server_idf
}  // namespace esphome
#endif  // defined(USE_ESP32) && defined(USE_WEBSERVER_OTA)

#include "esphome/core/defines.h"
#if defined(USE_ESP32) && defined(USE_WEBSERVER_OTA)
#include "multipart.h"
#include "utils.h"
#include "esphome/core/log.h"
#include <cstring>
#include "multipart_parser.h"

namespace esphome::web_server_idf {

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
  const char *field = current_header_field_.c_str();
  size_t field_len = current_header_field_.length();

  if (str_startswith_case_insensitive(field, field_len, "content-disposition")) {
    // Parse name and filename from Content-Disposition
    extract_header_param(value, length, "name", current_part_.name);
    extract_header_param(value, length, "filename", current_part_.filename);
  } else if (str_startswith_case_insensitive(field, field_len, "content-type")) {
    str_trim(value, length, current_part_.content_type);
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
bool str_startswith_case_insensitive(const char *str, size_t str_len, const char *prefix) {
  size_t prefix_len = strlen(prefix);
  if (str_len < prefix_len) {
    return false;
  }
  return str_ncmp_ci(str, prefix, prefix_len);
}

// Extract a parameter value from a header line
// Handles both quoted and unquoted values
// Assigns to out if found, clears out otherwise
void extract_header_param(const char *header, size_t header_len, const char *param, std::string &out) {
  size_t param_len = strlen(param);
  size_t search_pos = 0;

  while (search_pos < header_len) {
    // Look for param name
    const char *found = strcasestr_n(header + search_pos, header_len - search_pos, param);
    if (!found) {
      out.clear();
      return;
    }
    size_t pos = found - header;

    // Check if this is a word boundary (not part of another parameter)
    if (pos > 0 && header[pos - 1] != ' ' && header[pos - 1] != ';' && header[pos - 1] != '\t') {
      search_pos = pos + 1;
      continue;
    }

    // Move past param name
    pos += param_len;

    // Skip whitespace and find '='
    while (pos < header_len && (header[pos] == ' ' || header[pos] == '\t')) {
      pos++;
    }

    if (pos >= header_len || header[pos] != '=') {
      search_pos = pos;
      continue;
    }

    pos++;  // Skip '='

    // Skip whitespace after '='
    while (pos < header_len && (header[pos] == ' ' || header[pos] == '\t')) {
      pos++;
    }

    if (pos >= header_len) {
      out.clear();
      return;
    }

    // Check if value is quoted
    if (header[pos] == '"') {
      pos++;
      const char *end = static_cast<const char *>(memchr(header + pos, '"', header_len - pos));
      if (end) {
        out.assign(header + pos, end - (header + pos));
        return;
      }
      // Malformed - no closing quote
      out.clear();
      return;
    }

    // Unquoted value - find the end (semicolon, comma, or end of string)
    size_t end = pos;
    while (end < header_len && header[end] != ';' && header[end] != ',' && header[end] != ' ' && header[end] != '\t') {
      end++;
    }

    out.assign(header + pos, end - pos);
    return;
  }

  out.clear();
}

// Parse boundary from Content-Type header
// Returns true if boundary found, false otherwise
// boundary_start and boundary_len will point to the boundary value
bool parse_multipart_boundary(const char *content_type, const char **boundary_start, size_t *boundary_len) {
  if (!content_type) {
    return false;
  }

  size_t content_type_len = strlen(content_type);

  // Check for multipart/form-data (case-insensitive)
  if (!strcasestr_n(content_type, content_type_len, "multipart/form-data")) {
    return false;
  }

  // Look for boundary parameter
  const char *b = strcasestr_n(content_type, content_type_len, "boundary=");
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

// Trim whitespace from both ends, assign result to out
void str_trim(const char *str, size_t len, std::string &out) {
  const char *start = str;
  const char *end = str + len;
  while (start < end && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n'))
    start++;
  while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
    end--;
  out.assign(start, end - start);
}

}  // namespace esphome::web_server_idf
#endif  // defined(USE_ESP32) && defined(USE_WEBSERVER_OTA)

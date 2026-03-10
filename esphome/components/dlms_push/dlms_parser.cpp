#include "dlms_parser.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace esphome {
namespace dlms_push {

static const char *const TAG = "dlms_parser";

DlmsParser::DlmsParser() { this->load_default_patterns_(); }

void DlmsParser::load_default_patterns_() {
  auto add_pattern = [this](const std::string &name, int priority, const std::vector<AxdrPatternStep> &steps) {
    AxdrDescriptorPattern pat{name, priority, steps, 0};
    auto it = std::upper_bound(
        this->patterns_.begin(), this->patterns_.end(), pat,
        [](const AxdrDescriptorPattern &a, const AxdrDescriptorPattern &b) { return a.priority < b.priority; });
    this->patterns_.insert(it, pat);
  };

  // T1: TC,TO,TS,TV
  add_pattern("T1", 10,
              {{AxdrTokenType::EXPECT_TYPE_EXACT, DLMS_DATA_TYPE_UINT16},
               {AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED},
               {AxdrTokenType::EXPECT_OBIS6_TAGGED},
               {AxdrTokenType::EXPECT_SCALER_TAGGED},
               {AxdrTokenType::EXPECT_VALUE_GENERIC}});

  // T2: TO,TV,TSU
  add_pattern("T2", 10,
              {{AxdrTokenType::EXPECT_OBIS6_TAGGED},
               {AxdrTokenType::EXPECT_VALUE_GENERIC},
               {AxdrTokenType::EXPECT_STRUCTURE_N, 2},
               {AxdrTokenType::GOING_DOWN},
               {AxdrTokenType::EXPECT_SCALER_TAGGED},
               {AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED},
               {AxdrTokenType::GOING_UP}});

  // T3: TV,TC,TSU,TO
  add_pattern("T3", 10,
              {{AxdrTokenType::EXPECT_VALUE_GENERIC},
               {AxdrTokenType::EXPECT_TYPE_EXACT, DLMS_DATA_TYPE_UINT16},
               {AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED},
               {AxdrTokenType::EXPECT_STRUCTURE_N, 2},
               {AxdrTokenType::GOING_DOWN},
               {AxdrTokenType::EXPECT_SCALER_TAGGED},
               {AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED},
               {AxdrTokenType::GOING_UP},
               {AxdrTokenType::EXPECT_OBIS6_TAGGED}});

  // U.ZPA: F,C,O,A,TV
  add_pattern("U.ZPA", 10,
              {{AxdrTokenType::EXPECT_TO_BE_FIRST},
               {AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED},
               {AxdrTokenType::EXPECT_OBIS6_UNTAGGED},
               {AxdrTokenType::EXPECT_ATTR8_UNTAGGED},
               {AxdrTokenType::EXPECT_VALUE_GENERIC}});
}

size_t DlmsParser::parse(const uint8_t *buffer, size_t length, DlmsDataCallback callback, bool show_log) {
  if (buffer == nullptr || length == 0) {
    if (show_log)
      ESP_LOGV(TAG, "Buffer is null or empty");
    return 0;
  }

  this->buffer_ = buffer;
  this->buffer_len_ = length;
  this->pos_ = 0;
  this->callback_ = std::move(callback);
  this->show_log_ = show_log;
  this->objects_found_ = 0;

  if (this->show_log_)
    ESP_LOGD(TAG, "Starting to parse buffer of length %zu", length);

  // Skip to notification flag 0x0F
  while (this->pos_ < this->buffer_len_) {
    if (this->read_byte_() == 0x0F) {
      if (this->show_log_)
        ESP_LOGD(TAG, "Found notification flag 0x0F at position %zu", this->pos_ - 1);
      break;
    }
  }

  // Strictly skip Invoke-ID/Priority (5 bytes)
  for (int i = 0; i < 5 && this->pos_ < this->buffer_len_; i++) {
    this->pos_++;
  }

  // Check for datetime object before the data and skip it if present
  if (this->test_if_date_time_12b_()) {
    if (this->show_log_)
      ESP_LOGV(TAG, "Skipping datetime object at position %zu", this->pos_);
    this->pos_ += 12;
  }

  // First byte after flag should be the data type (usually Structure or Array)
  uint8_t start_type = this->read_byte_();
  if (start_type != DLMS_DATA_TYPE_STRUCTURE && start_type != DLMS_DATA_TYPE_ARRAY) {
    if (this->show_log_) {
      ESP_LOGW(TAG, "Expected STRUCTURE or ARRAY after header, found type %02X at position %zu", start_type,
               this->pos_ - 1);
    }

    return 0;
  }

  // Trigger recursive parsing
  bool success = this->parse_element_(start_type, 0);
  if (!success && this->show_log_) {
    ESP_LOGV(TAG, "Some errors occurred parsing DLMS data, or unexpected end of buffer.");
  }

  if (this->show_log_) {
    ESP_LOGD(TAG, "Parsing completed. Processed %zu bytes, found %zu objects", this->pos_, this->objects_found_);
  }

  return this->objects_found_;
}

uint8_t DlmsParser::read_byte_() {
  if (this->pos_ >= this->buffer_len_)
    return 0xFF;
  return this->buffer_[this->pos_++];
}

uint16_t DlmsParser::read_u16_() {
  if (this->pos_ + 1 >= this->buffer_len_)
    return 0xFFFF;
  uint16_t val = (this->buffer_[this->pos_] << 8) | this->buffer_[this->pos_ + 1];
  this->pos_ += 2;
  return val;
}

uint32_t DlmsParser::read_u32_() {
  if (this->pos_ + 3 >= this->buffer_len_)
    return 0xFFFFFFFF;
  uint32_t val = (this->buffer_[this->pos_] << 24) | (this->buffer_[this->pos_ + 1] << 16) |
                 (this->buffer_[this->pos_ + 2] << 8) | this->buffer_[this->pos_ + 3];
  this->pos_ += 4;
  return val;
}

bool DlmsParser::test_if_date_time_12b_() {
  if (this->pos_ + 12 > this->buffer_len_)
    return false;
  const uint8_t *buf = &this->buffer_[this->pos_];

  uint16_t year = (buf[0] << 8) | buf[1];
  if (year != 0x0000 && (year < 1970 || year > 2100))
    return false;
  if (buf[2] != 0xFF && (buf[2] < 1 || buf[2] > 12))
    return false;
  if (buf[3] != 0xFF && (buf[3] < 1 || buf[3] > 31))
    return false;
  if (buf[4] != 0xFF && (buf[4] < 1 || buf[4] > 7))
    return false;
  if (buf[5] != 0xFF && buf[5] > 23)
    return false;
  if (buf[6] != 0xFF && buf[6] > 59)
    return false;
  if (buf[7] != 0xFF && buf[7] > 59)
    return false;

  // Hundredths of second
  uint8_t ms = buf[8];
  if (ms != 0xFF && ms > 99)
    return false;

  // Deviation (timezone offset, signed, 2 bytes)
  uint16_t u_dev = (buf[9] << 8) | buf[10];
  int16_t s_dev = (int16_t) u_dev;
  return s_dev == (int16_t) 0x8000 || (s_dev >= -720 && s_dev <= 720);
}

int DlmsParser::get_data_type_size_(DlmsDataType type) {
  switch (type) {
    case DLMS_DATA_TYPE_NONE:
      return 0;
    case DLMS_DATA_TYPE_BOOLEAN:
    case DLMS_DATA_TYPE_INT8:
    case DLMS_DATA_TYPE_UINT8:
    case DLMS_DATA_TYPE_ENUM:
      return 1;
    case DLMS_DATA_TYPE_INT16:
    case DLMS_DATA_TYPE_UINT16:
      return 2;
    case DLMS_DATA_TYPE_INT32:
    case DLMS_DATA_TYPE_UINT32:
    case DLMS_DATA_TYPE_FLOAT32:
      return 4;
    case DLMS_DATA_TYPE_INT64:
    case DLMS_DATA_TYPE_UINT64:
    case DLMS_DATA_TYPE_FLOAT64:
      return 8;
    case DLMS_DATA_TYPE_DATETIME:
      return 12;
    case DLMS_DATA_TYPE_DATE:
      return 5;
    case DLMS_DATA_TYPE_TIME:
      return 4;
    default:
      return -1;  // Variable or complex
  }
}

bool DlmsParser::is_value_data_type_(DlmsDataType type) {
  switch (type) {
    case DLMS_DATA_TYPE_ARRAY:
    case DLMS_DATA_TYPE_STRUCTURE:
    case DLMS_DATA_TYPE_COMPACT_ARRAY:
      return false;
    case DLMS_DATA_TYPE_NONE:
    case DLMS_DATA_TYPE_BOOLEAN:
    case DLMS_DATA_TYPE_BIT_STRING:
    case DLMS_DATA_TYPE_INT32:
    case DLMS_DATA_TYPE_UINT32:
    case DLMS_DATA_TYPE_OCTET_STRING:
    case DLMS_DATA_TYPE_STRING:
    case DLMS_DATA_TYPE_BINARY_CODED_DESIMAL:
    case DLMS_DATA_TYPE_STRING_UTF8:
    case DLMS_DATA_TYPE_INT8:
    case DLMS_DATA_TYPE_INT16:
    case DLMS_DATA_TYPE_UINT8:
    case DLMS_DATA_TYPE_UINT16:
    case DLMS_DATA_TYPE_INT64:
    case DLMS_DATA_TYPE_UINT64:
    case DLMS_DATA_TYPE_ENUM:
    case DLMS_DATA_TYPE_FLOAT32:
    case DLMS_DATA_TYPE_FLOAT64:
    case DLMS_DATA_TYPE_DATETIME:
    case DLMS_DATA_TYPE_DATE:
    case DLMS_DATA_TYPE_TIME:
      return true;
    default:
      return false;
  }
}

bool DlmsParser::skip_data_(uint8_t type) {
  int data_size = this->get_data_type_size_((DlmsDataType) type);

  if (data_size == 0)
    return true;
  if (data_size > 0) {
    if (this->pos_ + data_size > this->buffer_len_)
      return false;
    this->pos_ += data_size;
  } else {
    uint8_t first_byte = this->read_byte_();
    if (first_byte == 0xFF)
      return false;

    uint32_t length = first_byte;
    // Handle DLMS multi-byte length fields
    if (first_byte > 127) {
      uint8_t num_bytes = first_byte & 0x7F;
      length = 0;
      for (int i = 0; i < num_bytes; i++) {
        uint8_t b = this->read_byte_();
        if (b == 0xFF && this->pos_ >= this->buffer_len_)
          return false;
        length = (length << 8) | b;
      }
    }

    uint32_t skip_bytes = length;
    // DLMS Bit strings designate their length in bits, so we must adjust the byte skip
    if (type == DLMS_DATA_TYPE_BIT_STRING) {
      skip_bytes = (length + 7) / 8;
    }

    if (this->pos_ + skip_bytes > this->buffer_len_)
      return false;

    this->pos_ += skip_bytes;
  }
  return true;
}

bool DlmsParser::parse_element_(uint8_t type, uint8_t depth) {
  if (type == DLMS_DATA_TYPE_STRUCTURE || type == DLMS_DATA_TYPE_ARRAY) {
    return this->parse_sequence_(type, depth);
  }
  return this->skip_data_(type);
}

bool DlmsParser::parse_sequence_(uint8_t type, uint8_t depth) {
  uint8_t elements_count = this->read_byte_();
  if (elements_count == 0xFF) {
    if (this->show_log_)
      ESP_LOGVV(TAG, "Invalid sequence length at position %zu", this->pos_ - 1);
    return false;
  }

  if (this->show_log_) {
    ESP_LOGD(TAG, "Parsing %s with %d elements at position %zu (depth %d)",
             type == DLMS_DATA_TYPE_STRUCTURE ? "STRUCTURE" : "ARRAY", elements_count, this->pos_ - 1, depth);
  }

  uint8_t elements_consumed = 0;
  while (elements_consumed < elements_count) {
    size_t original_position = this->pos_;

    if (this->try_match_patterns_(elements_consumed)) {
      elements_consumed += this->last_pattern_elements_consumed_ ? this->last_pattern_elements_consumed_ : 1;
      this->last_pattern_elements_consumed_ = 0;
      continue;
    }

    if (this->pos_ >= this->buffer_len_) {
      if (this->show_log_) {
        ESP_LOGV(TAG, "Unexpected end while reading element %d of %s", elements_consumed + 1,
                 type == DLMS_DATA_TYPE_STRUCTURE ? "STRUCTURE" : "ARRAY");
      }

      return false;
    }

    uint8_t elem_type = this->read_byte_();
    if (!this->parse_element_(elem_type, depth + 1))
      return false;
    elements_consumed++;

    if (this->pos_ == original_position) {
      if (this->show_log_) {
        ESP_LOGV(TAG, "No progress parsing element %d at position %zu, aborting to avoid infinite loop",
                 elements_consumed, original_position);
      }

      return false;
    }
  }
  return true;
}

bool DlmsParser::capture_generic_value_(AxdrCaptures &c) {
  uint8_t vt = this->read_byte_();
  if (!this->is_value_data_type_((DlmsDataType) vt))
    return false;

  int ds = this->get_data_type_size_((DlmsDataType) vt);
  if (ds > 0) {
    if (this->pos_ + ds > this->buffer_len_)
      return false;
    c.value_ptr = &this->buffer_[this->pos_];
    c.value_len = ds;
    this->pos_ += ds;
  } else if (ds == 0) {
    c.value_ptr = nullptr;
    c.value_len = 0;
  } else {
    uint8_t first_byte = this->read_byte_();
    if (first_byte == 0xFF)
      return false;

    uint32_t length = first_byte;
    if (first_byte > 127) {
      uint8_t num_bytes = first_byte & 0x7F;
      length = 0;
      for (int i = 0; i < num_bytes; i++) {
        uint8_t b = this->read_byte_();
        if (b == 0xFF && this->pos_ >= this->buffer_len_)
          return false;
        length = (length << 8) | b;
      }
    }

    uint32_t data_bytes = length;
    if (vt == DLMS_DATA_TYPE_BIT_STRING) {
      data_bytes = (length + 7) / 8;
    }

    if (this->pos_ + data_bytes > this->buffer_len_)
      return false;
    c.value_ptr = &this->buffer_[this->pos_];
    c.value_len = data_bytes > 255 ? 255 : data_bytes;
    this->pos_ += data_bytes;
  }
  c.value_type = (DlmsDataType) vt;
  return true;
}

bool DlmsParser::try_match_patterns_(uint8_t elem_idx) {
  for (const auto &p : this->patterns_) {
    uint8_t consumed = 0;
    size_t saved_position = this->pos_;
    if (this->match_pattern_(elem_idx, p, consumed)) {
      this->last_pattern_elements_consumed_ = consumed;
      return true;
    }
    this->pos_ = saved_position;  // Backtrack if match failed
  }
  return false;
}

bool DlmsParser::match_pattern_(uint8_t elem_idx, const AxdrDescriptorPattern &pat,
                                uint8_t &elements_consumed_at_level0) {
  AxdrCaptures cap{};
  elements_consumed_at_level0 = 0;
  uint8_t level = 0;
  auto consume_one = [&]() {
    if (level == 0)
      elements_consumed_at_level0++;
  };
  size_t initial_position = this->pos_;

  for (const auto &step : pat.steps) {
    switch (step.type) {
      case AxdrTokenType::EXPECT_TO_BE_FIRST:
        if (elem_idx != 0)
          return false;
        break;
      case AxdrTokenType::EXPECT_TYPE_EXACT:
        if (this->read_byte_() != step.param_u8_a)
          return false;
        consume_one();
        break;
      case AxdrTokenType::EXPECT_TYPE_U_I_8: {
        uint8_t t = this->read_byte_();
        if (t != DLMS_DATA_TYPE_INT8 && t != DLMS_DATA_TYPE_UINT8)
          return false;
        consume_one();
        break;
      }
      case AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED: {
        uint16_t v = this->read_u16_();
        if (v > 0x00FF)
          return false;  // Match max typical class ID
        cap.class_id = v;
        break;
      }
      case AxdrTokenType::EXPECT_OBIS6_TAGGED:
        if (this->read_byte_() != DLMS_DATA_TYPE_OCTET_STRING)
          return false;
        if (this->read_byte_() != 6)
          return false;
        if (this->pos_ + 6 > this->buffer_len_)
          return false;
        cap.obis = &this->buffer_[this->pos_];
        this->pos_ += 6;
        consume_one();
        break;
      case AxdrTokenType::EXPECT_OBIS6_UNTAGGED:
        if (this->pos_ + 6 > this->buffer_len_)
          return false;
        cap.obis = &this->buffer_[this->pos_];
        this->pos_ += 6;
        break;
      case AxdrTokenType::EXPECT_ATTR8_UNTAGGED:
        if (this->read_byte_() == 0)
          return false;
        break;
      case AxdrTokenType::EXPECT_VALUE_GENERIC:
        if (!this->capture_generic_value_(cap))
          return false;
        consume_one();
        break;
      case AxdrTokenType::EXPECT_STRUCTURE_N:
        if (this->read_byte_() != DLMS_DATA_TYPE_STRUCTURE)
          return false;
        if (this->read_byte_() != step.param_u8_a)
          return false;
        consume_one();
        break;
      case AxdrTokenType::EXPECT_SCALER_TAGGED:
        if (this->read_byte_() != DLMS_DATA_TYPE_INT8)
          return false;
        cap.scaler = (int8_t) this->read_byte_();
        cap.has_scaler_unit = true;
        consume_one();
        break;
      case AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED:
        if (this->read_byte_() != DLMS_DATA_TYPE_ENUM)
          return false;
        cap.unit_enum = this->read_byte_();
        cap.has_scaler_unit = true;
        consume_one();
        break;
      case AxdrTokenType::GOING_DOWN:
        level++;
        break;
      case AxdrTokenType::GOING_UP:
        level--;
        break;
    }
  }

  if (elements_consumed_at_level0 == 0)
    elements_consumed_at_level0 = 1;
  cap.elem_idx = initial_position;
  this->emit_object_(pat, cap);
  return true;
}

void DlmsParser::emit_object_(const AxdrDescriptorPattern &pat, const AxdrCaptures &c) {
  if (!c.obis || !this->callback_)
    return;

  // Use stack-allocated buffer for OBIS to avoid heap allocation
  char obis_str_buf[32];
  this->obis_to_string_(c.obis, obis_str_buf, sizeof(obis_str_buf));

  float raw_val_f = this->data_as_float_(c.value_type, c.value_ptr, c.value_len);
  float val_f = raw_val_f;

  bool is_numeric = (c.value_type != DLMS_DATA_TYPE_OCTET_STRING && c.value_type != DLMS_DATA_TYPE_STRING &&
                     c.value_type != DLMS_DATA_TYPE_STRING_UTF8);

  if (c.has_scaler_unit && is_numeric) {
    val_f *= std::pow(10, c.scaler);
  }

  if (this->show_log_) {
    ESP_LOGD(TAG, "Pattern match '%s' at idx %u ===============", pat.name.c_str(), c.elem_idx);
    uint16_t cid = c.class_id ? c.class_id : pat.default_class_id;

    ESP_LOGI(TAG, "Found attribute descriptor: class_id=%d, obis=%s", cid, obis_str_buf);

    if (c.value_ptr && c.value_len > 0) {
      char hex_buf[512];
      esphome::format_hex_pretty_to(hex_buf, sizeof(hex_buf), c.value_ptr, c.value_len);
      ESP_LOGI(TAG, " as hex dump : %s", hex_buf);
    }
    ESP_LOGI(TAG, " as number   : %f", raw_val_f);

    if (c.has_scaler_unit && is_numeric) {
      ESP_LOGI(TAG, " as number * scaler  : %f", val_f);
    }
  }

  this->callback_(obis_str_buf, val_f, is_numeric);
  this->objects_found_++;
}

float DlmsParser::data_as_float_(DlmsDataType value_type, const uint8_t *ptr, uint8_t len) {
  if (!ptr || len == 0)
    return 0.0f;

  auto be16 = [](const uint8_t *p) { return (uint16_t) ((p[0] << 8) | p[1]); };
  auto be32 = [](const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | (uint32_t) p[3];
  };
  auto be64 = [](const uint8_t *p) {
    return ((uint64_t) p[0] << 56) | ((uint64_t) p[1] << 48) | ((uint64_t) p[2] << 40) | ((uint64_t) p[3] << 32) |
           ((uint64_t) p[4] << 24) | ((uint64_t) p[5] << 16) | ((uint64_t) p[6] << 8) | (uint64_t) p[7];
  };

  switch (value_type) {
    case DLMS_DATA_TYPE_BOOLEAN:
    case DLMS_DATA_TYPE_ENUM:
    case DLMS_DATA_TYPE_UINT8:
      return static_cast<float>(ptr[0]);
    case DLMS_DATA_TYPE_INT8:
      return static_cast<float>(static_cast<int8_t>(ptr[0]));
    case DLMS_DATA_TYPE_BIT_STRING:
      return (len > 0 && ptr) ? static_cast<float>(ptr[0]) : 0.0f;
    case DLMS_DATA_TYPE_UINT16:
      return len >= 2 ? static_cast<float>(be16(ptr)) : 0.0f;
    case DLMS_DATA_TYPE_INT16:
      return len >= 2 ? static_cast<float>(static_cast<int16_t>(be16(ptr))) : 0.0f;
    case DLMS_DATA_TYPE_UINT32:
      return len >= 4 ? static_cast<float>(be32(ptr)) : 0.0f;
    case DLMS_DATA_TYPE_INT32:
      return len >= 4 ? static_cast<float>(static_cast<int32_t>(be32(ptr))) : 0.0f;
    case DLMS_DATA_TYPE_UINT64:
      return len >= 8 ? static_cast<float>(be64(ptr)) : 0.0f;
    case DLMS_DATA_TYPE_INT64:
      return len >= 8 ? static_cast<float>(static_cast<int64_t>(be64(ptr))) : 0.0f;
    case DLMS_DATA_TYPE_FLOAT32: {
      if (len < 4)
        return 0.0f;
      uint32_t i32 = be32(ptr);
      float f;
      std::memcpy(&f, &i32, sizeof(float));
      return f;
    }
    case DLMS_DATA_TYPE_FLOAT64: {
      if (len < 8)
        return 0.0f;
      uint64_t i64 = be64(ptr);
      double d;
      std::memcpy(&d, &i64, sizeof(double));
      return static_cast<float>(d);
    }
    default:
      return 0.0f;
  }
}

void DlmsParser::obis_to_string_(const uint8_t *obis, char *buffer, size_t max_len) {
  if (max_len > 0)
    buffer[0] = '\0';
  if (!obis || max_len == 0)
    return;
  snprintf(buffer, max_len, "%u.%u.%u.%u.%u.%u", obis[0], obis[1], obis[2], obis[3], obis[4], obis[5]);
}

}  // namespace dlms_push
}  // namespace esphome

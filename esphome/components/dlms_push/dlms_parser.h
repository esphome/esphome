#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace esphome {
namespace dlms_push {

enum DlmsDataType : uint8_t {
  DLMS_DATA_TYPE_NONE = 0,
  DLMS_DATA_TYPE_ARRAY = 1,
  DLMS_DATA_TYPE_STRUCTURE = 2,
  DLMS_DATA_TYPE_BOOLEAN = 3,
  DLMS_DATA_TYPE_BIT_STRING = 4,
  DLMS_DATA_TYPE_INT32 = 5,
  DLMS_DATA_TYPE_UINT32 = 6,
  DLMS_DATA_TYPE_OCTET_STRING = 9,
  DLMS_DATA_TYPE_STRING = 10,
  DLMS_DATA_TYPE_STRING_UTF8 = 12,
  DLMS_DATA_TYPE_BINARY_CODED_DESIMAL = 13,
  DLMS_DATA_TYPE_INT8 = 15,
  DLMS_DATA_TYPE_INT16 = 16,
  DLMS_DATA_TYPE_UINT8 = 17,
  DLMS_DATA_TYPE_UINT16 = 18,
  DLMS_DATA_TYPE_COMPACT_ARRAY = 19,
  DLMS_DATA_TYPE_INT64 = 20,
  DLMS_DATA_TYPE_UINT64 = 21,
  DLMS_DATA_TYPE_ENUM = 22,
  DLMS_DATA_TYPE_FLOAT32 = 23,
  DLMS_DATA_TYPE_FLOAT64 = 24,
  DLMS_DATA_TYPE_DATETIME = 25,
  DLMS_DATA_TYPE_DATE = 26,
  DLMS_DATA_TYPE_TIME = 27
};

// Callback for the hub: OBIS code (e.g. "1.0.1.8.0.255"), numeric value, string value, is_numeric flag
using DlmsDataCallback =
    std::function<void(const char *obis_code, float float_val, const char *str_val, bool is_numeric)>;

// --- Pattern Matching Enums & Structs ---
enum class AxdrTokenType : uint8_t {
  EXPECT_TO_BE_FIRST,
  EXPECT_TYPE_EXACT,
  EXPECT_TYPE_U_I_8,
  EXPECT_CLASS_ID_UNTAGGED,
  EXPECT_OBIS6_TAGGED,
  EXPECT_OBIS6_UNTAGGED,
  EXPECT_ATTR8_UNTAGGED,
  EXPECT_VALUE_GENERIC,
  EXPECT_STRUCTURE_N,
  EXPECT_SCALER_TAGGED,
  EXPECT_UNIT_ENUM_TAGGED,
  GOING_DOWN,
  GOING_UP,
};

struct AxdrPatternStep {
  AxdrTokenType type;
  uint8_t param_u8_a{0};
};

struct AxdrDescriptorPattern {
  std::string name;
  int priority{0};
  std::vector<AxdrPatternStep> steps;
  uint16_t default_class_id{0};
};

struct AxdrCaptures {
  uint32_t elem_idx{0};
  uint16_t class_id{0};
  const uint8_t *obis{nullptr};
  DlmsDataType value_type{DlmsDataType::DLMS_DATA_TYPE_NONE};
  const uint8_t *value_ptr{nullptr};
  uint8_t value_len{0};

  bool has_scaler_unit{false};
  int8_t scaler{0};
  uint8_t unit_enum{0};
};

class DlmsParser {
 public:
  DlmsParser();

  // Registers a custom parsing pattern from the YAML config
  void register_custom_pattern(const std::string &dsl);

  // Parses the buffer and fires callbacks for each found sensor value
  size_t parse(const uint8_t *buffer, size_t length, DlmsDataCallback callback, bool show_log);

 private:
  void register_pattern_dsl_(const std::string &name, const std::string &dsl, int priority);
  void load_default_patterns_();

  uint8_t read_byte_();
  uint16_t read_u16_();
  uint32_t read_u32_();

  bool test_if_date_time_12b_();
  int get_data_type_size_(DlmsDataType type);
  bool is_value_data_type_(DlmsDataType type);

  bool skip_data_(uint8_t type);
  bool parse_element_(uint8_t type, uint8_t depth = 0);
  bool parse_sequence_(uint8_t type, uint8_t depth = 0);

  bool capture_generic_value_(AxdrCaptures &c);
  bool try_match_patterns_(uint8_t elem_idx);
  bool match_pattern_(uint8_t elem_idx, const AxdrDescriptorPattern &pat, uint8_t &elements_consumed_at_level0);
  void emit_object_(const AxdrDescriptorPattern &pat, const AxdrCaptures &c);

  float data_as_float_(DlmsDataType value_type, const uint8_t *ptr, uint8_t len);
  void data_to_string_(DlmsDataType value_type, const uint8_t *ptr, uint8_t len, char *buffer, size_t max_len);
  void obis_to_string_(const uint8_t *obis, char *buffer, size_t max_len);
  const char *dlms_data_type_to_string_(DlmsDataType vt);

  const uint8_t *buffer_{nullptr};
  size_t buffer_len_{0};

  size_t pos_{0};
  DlmsDataCallback callback_;
  bool show_log_{false};
  size_t objects_found_{0};
  uint8_t last_pattern_elements_consumed_{0};

  std::vector<AxdrDescriptorPattern> patterns_;
};

}  // namespace dlms_push
}  // namespace esphome

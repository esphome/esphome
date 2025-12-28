#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Copy the implementations to test them in isolation

inline char format_hex_char(uint8_t v, char base) { return v >= 10 ? base + (v - 10) : '0' + v; }
inline char format_hex_char(uint8_t v) { return format_hex_char(v, 'a'); }
inline char format_hex_pretty_char(uint8_t v) { return format_hex_char(v, 'A'); }

constexpr size_t format_hex_pretty_size(size_t byte_count) { return byte_count * 3; }

static char *format_hex_internal(char *buffer, size_t buffer_size, const uint8_t *data, size_t length, char separator,
                                 char base) {
  if (length == 0) {
    buffer[0] = '\0';
    return buffer;
  }
  uint8_t stride = separator ? 3 : 2;
  size_t max_bytes = separator ? (buffer_size / stride) : ((buffer_size - 1) / stride);
  if (max_bytes == 0) {
    buffer[0] = '\0';
    return buffer;
  }
  if (length > max_bytes) {
    length = max_bytes;
  }
  for (size_t i = 0; i < length; i++) {
    size_t pos = i * stride;
    buffer[pos] = format_hex_char(data[i] >> 4, base);
    buffer[pos + 1] = format_hex_char(data[i] & 0x0F, base);
    if (separator && i < length - 1) {
      buffer[pos + 2] = separator;
    }
  }
  buffer[length * stride - (separator ? 1 : 0)] = '\0';
  return buffer;
}

char *format_hex_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t length) {
  return format_hex_internal(buffer, buffer_size, data, length, 0, 'a');
}

char *format_hex_pretty_to(char *buffer, size_t buffer_size, const uint8_t *data, size_t length, char separator) {
  return format_hex_internal(buffer, buffer_size, data, length, separator, 'A');
}

template<size_t N>
char *format_hex_pretty_to(char (&buffer)[N], const uint8_t *data, size_t length, char separator = ':') {
  return format_hex_pretty_to(buffer, N, data, length, separator);
}

static constexpr size_t MAC_ADDRESS_SIZE = 6;
static constexpr size_t MAC_ADDRESS_PRETTY_BUFFER_SIZE = format_hex_pretty_size(MAC_ADDRESS_SIZE);
static constexpr size_t MAC_ADDRESS_BUFFER_SIZE = MAC_ADDRESS_SIZE * 2 + 1;

inline void format_mac_addr_upper(const uint8_t *mac, char *output) {
  format_hex_pretty_to(output, MAC_ADDRESS_PRETTY_BUFFER_SIZE, mac, MAC_ADDRESS_SIZE, ':');
}

inline void format_mac_addr_lower_no_sep(const uint8_t *mac, char *output) {
  format_hex_to(output, MAC_ADDRESS_BUFFER_SIZE, mac, MAC_ADDRESS_SIZE);
}

// Tests

void test_format_hex_char_base() {
  assert(format_hex_char(0, 'a') == '0');
  assert(format_hex_char(9, 'a') == '9');
  assert(format_hex_char(10, 'a') == 'a');
  assert(format_hex_char(15, 'a') == 'f');
  assert(format_hex_char(0, 'A') == '0');
  assert(format_hex_char(10, 'A') == 'A');
  assert(format_hex_char(15, 'A') == 'F');
  printf("✓ format_hex_char with base\n");
}

void test_format_hex_char_lowercase() {
  assert(format_hex_char(0) == '0');
  assert(format_hex_char(10) == 'a');
  assert(format_hex_char(15) == 'f');
  printf("✓ format_hex_char lowercase\n");
}

void test_format_hex_pretty_char_uppercase() {
  assert(format_hex_pretty_char(0) == '0');
  assert(format_hex_pretty_char(10) == 'A');
  assert(format_hex_pretty_char(15) == 'F');
  printf("✓ format_hex_pretty_char uppercase\n");
}

void test_format_hex_to() {
  uint8_t data[] = {0xde, 0xad, 0xbe, 0xef};
  char buf[9];
  format_hex_to(buf, sizeof(buf), data, 4);
  assert(strcmp(buf, "deadbeef") == 0);
  printf("✓ format_hex_to lowercase\n");
}

void test_format_hex_pretty_to_colon() {
  uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  char buf[12];
  format_hex_pretty_to(buf, sizeof(buf), data, 4, ':');
  assert(strcmp(buf, "DE:AD:BE:EF") == 0);
  printf("✓ format_hex_pretty_to with colon\n");
}

void test_format_hex_pretty_to_dot() {
  uint8_t data[] = {0xAA, 0xBB, 0xCC};
  char buf[9];
  format_hex_pretty_to(buf, sizeof(buf), data, 3, '.');
  assert(strcmp(buf, "AA.BB.CC") == 0);
  printf("✓ format_hex_pretty_to with dot\n");
}

void test_buffer_overflow_protection() {
  uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  char buf[11];
  format_hex_pretty_to(buf, 11, data, 5, ':');
  assert(strcmp(buf, "AA:BB:CC") == 0);
  printf("✓ buffer overflow protection\n");
}

void test_exact_fit() {
  uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
  char buf[12];
  format_hex_pretty_to(buf, 12, data, 4, ':');
  assert(strcmp(buf, "AA:BB:CC:DD") == 0);
  printf("✓ exact fit buffer\n");
}

void test_mac_addr_upper() {
  uint8_t mac[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
  char buf[18];
  format_mac_addr_upper(mac, buf);
  assert(strcmp(buf, "AA:BB:CC:DD:EE:FF") == 0);
  printf("✓ format_mac_addr_upper\n");
}

void test_mac_addr_lower_no_sep() {
  uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  char buf[13];
  format_mac_addr_lower_no_sep(mac, buf);
  assert(strcmp(buf, "aabbccddeeff") == 0);
  printf("✓ format_mac_addr_lower_no_sep\n");
}

void test_empty() {
  uint8_t data[] = {0xAA};
  char buf[12];
  format_hex_pretty_to(buf, sizeof(buf), data, 0, ':');
  assert(strcmp(buf, "") == 0);
  printf("✓ empty data\n");
}

void test_single_byte() {
  uint8_t data[] = {0x42};
  char buf[3];
  format_hex_pretty_to(buf, sizeof(buf), data, 1, ':');
  assert(strcmp(buf, "42") == 0);
  printf("✓ single byte\n");
}

void test_template() {
  uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  char buf[format_hex_pretty_size(4)];
  format_hex_pretty_to(buf, data, 4);
  assert(strcmp(buf, "DE:AD:BE:EF") == 0);
  printf("✓ template version\n");
}

void test_no_separator() {
  uint8_t data[] = {0xDE, 0xAD};
  char buf[5];
  format_hex_pretty_to(buf, sizeof(buf), data, 2, 0);
  assert(strcmp(buf, "DEAD") == 0);
  printf("✓ no separator\n");
}

void test_constexpr() {
  static_assert(format_hex_pretty_size(1) == 3, "");
  static_assert(format_hex_pretty_size(4) == 12, "");
  static_assert(format_hex_pretty_size(6) == 18, "");
  static_assert(MAC_ADDRESS_SIZE == 6, "");
  static_assert(MAC_ADDRESS_PRETTY_BUFFER_SIZE == 18, "");
  static_assert(MAC_ADDRESS_BUFFER_SIZE == 13, "");
  printf("✓ constexpr values\n");
}

int main() {
  printf("Running hex formatting tests...\n\n");

  test_format_hex_char_base();
  test_format_hex_char_lowercase();
  test_format_hex_pretty_char_uppercase();
  test_format_hex_to();
  test_format_hex_pretty_to_colon();
  test_format_hex_pretty_to_dot();
  test_buffer_overflow_protection();
  test_exact_fit();
  test_mac_addr_upper();
  test_mac_addr_lower_no_sep();
  test_empty();
  test_single_byte();
  test_template();
  test_no_separator();
  test_constexpr();

  printf("\n✅ All 15 tests passed!\n");
  return 0;
}

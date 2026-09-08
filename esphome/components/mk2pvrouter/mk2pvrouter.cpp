#include "mk2pvrouter.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter";

constexpr uint8_t START_FRAME = 0x2;
constexpr uint8_t END_FRAME = 0x3;
constexpr uint8_t LINE_FEED = 0xa;
constexpr uint8_t CARRIAGE_RETURN = 0xd;
constexpr uint8_t TAB = 0x9;
constexpr uint8_t MAX_ITERATIONS = 128;
constexpr uint8_t CRC_MASK = 0x3F;
constexpr uint8_t CRC_OFFSET = 0x20;

// Extracts a TAB-delimited field from [buf_start, buf_end) into dest.
// Returns the field length, or 0 if no TAB was found, or the (uncopied) field
// length if it's >= max_len.
static size_t get_field(char *dest, const char *buf_start, const char *buf_end, size_t max_len) {
  const auto *const field_end = static_cast<const char *>(memchr(buf_start, TAB, buf_end - buf_start));
  if (!field_end)
    return 0;
  const size_t len = field_end - buf_start;
  if (len >= max_len) {
    ESP_LOGE(TAG, "Field too long: %zu bytes (max %zu)", len, max_len);
    return len;
  }

  memcpy(dest, buf_start, len);
  dest[len] = '\0';  // Null-terminate
  return len;
}

// Calculates the CRC (checksum) for a given group of characters.
uint8_t Mk2PVRouter::calculate_crc_(const char *grp, size_t grp_len) {
  uint8_t crc_tmp{0};
  const auto effective_len = grp_len - CRC_SUFFIX_LEN;
  for (size_t i = 0; i < effective_len; i++) {
    crc_tmp += grp[i];
  }
  crc_tmp &= CRC_MASK;
  crc_tmp += CRC_OFFSET;
  return crc_tmp;
}

// Verifies the CRC of a group against its trailing CRC byte.
bool Mk2PVRouter::check_crc_(const char *grp, const char *grp_end) {
  const auto grp_len = grp_end - grp;
  if (grp_len < static_cast<decltype(grp_len)>(CRC_SUFFIX_LEN)) {
    ESP_LOGE(TAG, "Empty or too short group");
    return false;
  }
  const auto raw_crc = grp[grp_len - 1];

  const auto calculated_crc = this->calculate_crc_(grp, grp_len);

  if (raw_crc != calculated_crc) {
    ESP_LOGE(TAG, "CRC mismatch: expected %d, got %d", calculated_crc, raw_crc);
    return false;
  }
  return true;
}

// Validates, parses, and publishes a single tag/value group.
void Mk2PVRouter::process_group_(const char *grp, const char *grp_end) {
  if (!this->check_crc_(grp, grp_end))
    return;

  size_t field_len = get_field(this->tag_, grp, grp_end, MAX_TAG_SIZE);
  if (!field_len || field_len >= MAX_TAG_SIZE) {
    ESP_LOGE(TAG, "Invalid tag");
    return;
  }
  const auto *val_start = grp + field_len + 1;  // Skip tag + TAB.

  field_len = get_field(this->val_, val_start, grp_end, MAX_VAL_SIZE);
  if (!field_len || field_len >= MAX_VAL_SIZE) {
    ESP_LOGE(TAG, "Invalid value for tag %s", this->tag_);
    return;
  }

  this->publish_value_(this->tag_, this->val_);
}

// Reads characters until `c` is found or the internal buffer is full.
bool Mk2PVRouter::read_chars_until_(bool drop, uint8_t c) {
  size_t j{0};

  while (this->available() > 0 && j++ < MAX_ITERATIONS) {
    const auto received = this->read();
    if (received < 0)
      continue;
    if (received == c)
      return true;
    if (drop)
      continue;
    if (this->buf_index_ >= (sizeof(this->buf_) - 1)) {
      ESP_LOGW(TAG, "Internal buffer full");
      this->buf_index_ = 0;
      this->state_ = State::WAITING_FOR_START;
      return false;
    }
    this->buf_[this->buf_index_++] = received;
  }

  return false;
}

void Mk2PVRouter::loop() {
  switch (this->state_) {
    case State::WAITING_FOR_START:
      ESP_LOGVV(TAG, "State: WAITING_FOR_START");
      if (this->read_chars_until_(true, START_FRAME))
        this->state_ = State::START_FRAME_RECEIVED;
      break;
    case State::START_FRAME_RECEIVED:
      ESP_LOGVV(TAG, "State: START_FRAME_RECEIVED");
      if (this->read_chars_until_(false, END_FRAME))
        this->state_ = State::END_FRAME_RECEIVED;
      break;
    case State::END_FRAME_RECEIVED: {
      ESP_LOGVV(TAG, "State: END_FRAME_RECEIVED -> processing");

      if (this->buf_index_ == 0) {
        this->state_ = State::WAITING_FOR_START;
        break;
      }

      auto *buf_finger = this->buf_;
      auto *buf_end = this->buf_ + this->buf_index_;

      // Each group: 0xa(LF) | Tag | 0x9(TAB) | Data | 0x9(TAB) | CRC | 0xd(CR)
      // CRC is computed over "Tag | TAB | Data | TAB".
      while ((buf_finger = static_cast<char *>(memchr(buf_finger, LINE_FEED, buf_end - buf_finger))) != nullptr) {
        ++buf_finger;  // Skip LF to the start of the group.

        auto *const grp_end = static_cast<char *>(memchr(buf_finger, CARRIAGE_RETURN, buf_end - buf_finger));
        if (!grp_end) {
          ESP_LOGE(TAG, "No group found");
          break;
        }

        this->process_group_(buf_finger, grp_end);

        buf_finger = grp_end;  // grp_end is always < buf_end, so this stays in bounds.
      }
      this->buf_index_ = 0;
      this->state_ = State::WAITING_FOR_START;
      break;
    }
  }
}

void Mk2PVRouter::publish_value_(const char *tag, const char *val) {
#ifdef MK2PVROUTER_LISTENER_COUNT
  for (auto *element : this->mk2pvrouter_listeners_) {
    if (strcmp(tag, element->get_tag()) != 0)
      continue;
    element->publish_val(val);
  }
#endif
}

void Mk2PVRouter::dump_config() { ESP_LOGCONFIG(TAG, "Mk2PVRouter:"); }

#ifdef MK2PVROUTER_LISTENER_COUNT
void Mk2PVRouter::register_mk2pvrouter_listener(Mk2PVRouterListener *listener) {
  this->mk2pvrouter_listeners_.push_back(listener);
}
#endif

}  // namespace esphome::mk2pvrouter

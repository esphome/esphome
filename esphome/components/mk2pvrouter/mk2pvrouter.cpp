#include "mk2pvrouter.h"
#include "esphome/core/log.h"

namespace esphome::mk2pvrouter {

static const char *const TAG = "mk2pvrouter";

constexpr uint8_t START_FRAME = 0x2;
constexpr uint8_t END_FRAME = 0x3;
constexpr uint8_t LINE_FEED = 0xa;
constexpr uint8_t CARRIAGE_RETURN = 0xd;
constexpr uint8_t TAB = 0x9;
constexpr uint8_t MAX_ITERATIONS = 128;

/**
 * @brief Extracts a field (substring) from a buffer, delimited by a TAB character (0x9).
 *
 * @param dest Pointer to the destination buffer where the extracted field will be stored.
 * @param buf_start Pointer to the start of the buffer to extract the field from.
 * @param buf_end Pointer to the end of the buffer.
 * @param max_len Maximum length of the destination buffer (including null terminator).
 * @return size_t Length of the extracted field if successful, 0 if no TAB delimiter is found,
 *                or the length of the field if it exceeds max_len (but the field is not copied).
 */
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

/**
 * @brief Calculates the CRC (checksum) for a given group of characters.
 *
 * @param grp Pointer to the start of the group.
 * @param grp_len Length of the group.
 * @return uint8_t The calculated CRC value.
 */
uint8_t Mk2PVRouter::calculate_crc_(const char *grp, size_t grp_len) {
  uint8_t crc_tmp{0};
  const auto effective_len = grp_len - CHECKSUM_AREA_END;
  for (size_t i = 0; i < effective_len; i++) {
    crc_tmp += grp[i];
  }
  crc_tmp &= 0x3F;
  crc_tmp += 0x20;
  return crc_tmp;
}

/**
 * @brief Verifies the CRC of a group by comparing the calculated CRC with the provided CRC.
 *
 * @param grp Pointer to the start of the group.
 * @param grp_end Pointer to the end of the group.
 * @return true If the CRC matches.
 * @return false If there is a mismatch.
 * @note Logs an error message if the CRC does not match.
 */
bool Mk2PVRouter::check_crc_(const char *grp, const char *grp_end) {
  const auto grp_len = grp_end - grp;
  const auto raw_crc = grp[grp_len - 1];

  const auto calculated_crc = calculate_crc_(grp, grp_len);

  if (raw_crc != calculated_crc) {
    ESP_LOGE(TAG, "CRC mismatch: expected %d, got %d", calculated_crc, raw_crc);
    return false;
  }
  return true;
}

/**
 * @brief Reads characters from the input buffer until a specific character is found or the buffer is full.
 *
 * @param drop If true, discards characters until the target character is found.
 * @param c The target character to stop reading at.
 * @return true If the target character is found.
 * @return false If the buffer is full or the target character is not found.
 * @note Logs a warning if the internal buffer is full.
 */
bool Mk2PVRouter::read_chars_until_(bool drop, uint8_t c) {
  size_t j{0};

  while (this->available() > 0 && j++ < MAX_ITERATIONS) {
    const auto received = this->read();
    if (received == c)
      return true;
    if (drop)
      continue;
    /*
     * Internal buffer is full, switch to OFF mode.
     * Data will be retrieved on next update.
     */
    if (this->buf_index_ >= (sizeof(this->buf_) - 1)) {
      ESP_LOGW(TAG, "Internal buffer full");
      this->state_ = State::OFF;
      return false;
    }
    this->buf_[this->buf_index_++] = received;
  }

  return false;
}

/**
 * @brief Initializes the Mk2PVRouter by setting the initial state to OFF.
 */
void Mk2PVRouter::setup() { this->state_ = State::OFF; }

/**
 * @brief Updates the Mk2PVRouter state. Resets the buffer index and transitions the state from OFF to ON.
 */
void Mk2PVRouter::update() {
  if (this->state_ == State::OFF) {
    this->buf_index_ = 0;
    this->state_ = State::ON;
  }
}

/**
 * @brief Implements the main state machine for processing incoming data.
 *
 * @details The state machine transitions through the following states:
 * - OFF: Does nothing.
 * - ON: Reads characters until the start frame (0x2) is found.
 * - START_FRAME_RECEIVED: Reads characters until the end frame (0x3) is found.
 * - END_FRAME_RECEIVED: Processes the buffer to extract groups, validate CRC, and publish values.
 */
void Mk2PVRouter::loop() {
  switch (this->state_) {
    case State::OFF:
      break;
    case State::ON:
      ESP_LOGVV(TAG, "State transition: ON -> START_FRAME_RECEIVED");
      /* Dequeue chars until start frame (0x2) */
      if (this->read_chars_until_(true, START_FRAME))
        this->state_ = State::START_FRAME_RECEIVED;
      break;
    case State::START_FRAME_RECEIVED:
      ESP_LOGVV(TAG, "State transition: START_FRAME_RECEIVED -> END_FRAME_RECEIVED");
      /* Dequeue chars until end frame (0x3) */
      if (this->read_chars_until_(false, END_FRAME))
        this->state_ = State::END_FRAME_RECEIVED;
      break;
    case State::END_FRAME_RECEIVED:
      ESP_LOGVV(TAG, "State transition: END_FRAME_RECEIVED -> DoWork");

      if (this->buf_index_ == 0) {
        this->state_ = State::OFF;
        break;
      }

      size_t field_len;

      auto *buf_finger = this->buf_;
      auto *buf_end = this->buf_ + this->buf_index_;

      /* Each frame is composed of multiple groups starting by 0xa(Line Feed) and ending by
       * 0xd ('\r').
       *
       * Each group contains tag, data and a CRC separated by 0x9 (\t)
       * 0xa | Tag | 0x9 | Data | 0x9 | CRC | 0xd
       *     ^^^^^^^^^^^^^^^^^^^^^^^^^
       * Checksum is computed on the above in standard mode.
       *
       */
      while ((buf_finger = static_cast<char *>(memchr(buf_finger, LINE_FEED, buf_end - buf_finger))) != nullptr) {
        /* Point to the first char of the group after 0xa */
        ++buf_finger;

        /* Group len */
        auto *const grp_end = static_cast<char *>(memchr(buf_finger, CARRIAGE_RETURN, buf_end - buf_finger));
        if (!grp_end) {
          ESP_LOGE(TAG, "No group found");
          break;
        }

        if (!this->check_crc_(buf_finger, grp_end))
          continue;

        /* Get tag */
        field_len = get_field(this->tag_, buf_finger, grp_end, MAX_TAG_SIZE);
        if (!field_len || field_len >= MAX_TAG_SIZE) {
          ESP_LOGE(TAG, "Invalid tag.");
          continue;
        }

        /* Advance buf_finger to after the tag and the separator. */
        buf_finger += field_len + 1;

        field_len = get_field(this->val_, buf_finger, grp_end, MAX_VAL_SIZE);
        if (!field_len || field_len >= MAX_VAL_SIZE) {
          ESP_LOGE(TAG, "Invalid value for tag %s", this->tag_);
          continue;
        }

        /* Advance buf_finger to end of group */
        /* Skip: value field + TAB + CRC + CR */
        buf_finger += field_len + 1 + 1 + 1;

        this->publish_value_(this->tag_, this->val_);
      }
      this->state_ = State::OFF;
      break;
  }
}

/**
 * @brief Publishes a value to all registered listeners that match the given tag.
 *
 * @param tag The tag associated with the value.
 * @param val The value to publish.
 */
void Mk2PVRouter::publish_value_(const char *tag, const char *val) {
  for (auto *element : this->mk2pvrouter_listeners_) {
    if (strcmp(tag, element->get_tag()) != 0)
      continue;
    element->publish_val(val);
  }
}

/**
 * @brief Dumps the Mk2PVRouter configuration to the log.
 *
 * @note Logs the UART settings and other configuration details.
 */
void Mk2PVRouter::dump_config() {
  ESP_LOGCONFIG(TAG, "Mk2PVRouter:");
  this->check_uart_settings(BAUD_RATE, 1, uart::UART_CONFIG_PARITY_EVEN, 7);
}

/**
 * @brief Registers a listener to receive updates for specific tags.
 *
 * @param listener Pointer to the listener to register.
 */
void Mk2PVRouter::register_mk2pvrouter_listener(Mk2PVRouterListener *listener) {
  this->mk2pvrouter_listeners_.push_back(listener);
}

}  // namespace esphome::mk2pvrouter

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace esphome::esp32_ble {

/// A 31 byte advertising or scan response payload, packed in the order and with the fit rules of Bluedroid's
/// btm_ble_build_adv_data, so the raw bytes match what the esp_ble_adv_data_t API put on air.
class AdvPayload {
 public:
  static constexpr size_t MAX_LEN = 31;
  using UUID128 = std::array<uint8_t, 16>;  // little endian, as esp_bt_uuid_t stores it

  const uint8_t *data() const { return this->buf_.data(); }
  size_t size() const { return this->len_; }

  void add_flags(uint8_t flags) {
    if (flags != 0 && this->left_() >= 3)
      this->add_(0x01, &flags, 1);
  }
  void add_appearance(uint16_t appearance) {
    const uint8_t value[2] = {static_cast<uint8_t>(appearance), static_cast<uint8_t>(appearance >> 8)};
    if (appearance != 0 && this->left_() > 3)
      this->add_(0x19, value, sizeof(value));
  }
  /// Complete name, or the shortened name type with what fits.
  void add_name(const char *name, size_t len) {
    if (this->left_() <= 2)
      return;
    const size_t fit = std::min(len, this->left_() - 2);
    this->add_(fit < len ? 0x08 : 0x09, reinterpret_cast<const uint8_t *>(name), fit);
  }
  /// Truncated to what fits.
  void add_manufacturer_data(std::span<const uint8_t> data) {
    if (!data.empty() && this->left_() > 2)
      this->add_(0xFF, data.data(), std::min(data.size(), this->left_() - 2));
  }
  void add_tx_power(int8_t dbm) {
    if (this->left_() > 2)
      this->add_(0x0A, reinterpret_cast<const uint8_t *>(&dbm), 1);
  }
  /// Bluetooth base UUIDs go in the 16 and 32-bit lists (partial when they do not all fit), then the first other one.
  void add_uuids(std::span<const UUID128> uuids) {
    this->add_short_uuids_(uuids, 2, 0x02);
    this->add_short_uuids_(uuids, 4, 0x04);
    if (this->left_() < 18)
      return;
    for (const auto &uuid : uuids) {
      if (uuid_size(uuid) == 16) {
        this->add_(0x07, uuid.data(), 16);
        return;
      }
    }
  }
  /// Raw service data element (its first bytes are the UUID); dropped whole when it does not fit.
  void add_service_data(std::span<const uint8_t> data) {
    if (!data.empty() && this->left_() >= data.size() + 2)
      this->add_(0x16, data.data(), data.size());
  }

  /// 2 or 4 for a Bluetooth base UUID, else 16.
  static size_t uuid_size(const UUID128 &uuid) {
    static constexpr uint8_t BASE[12] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00};
    if (memcmp(uuid.data(), BASE, sizeof(BASE)) != 0)
      return 16;
    return uuid[14] == 0 && uuid[15] == 0 ? 2 : 4;
  }

 protected:
  size_t left_() const { return MAX_LEN - this->len_; }
  void add_(uint8_t type, const uint8_t *value, size_t len) {
    this->buf_[this->len_++] = len + 1;
    this->buf_[this->len_++] = type;
    memcpy(this->buf_.data() + this->len_, value, len);
    this->len_ += len;
  }
  void add_short_uuids_(std::span<const UUID128> uuids, size_t size, uint8_t partial_type) {
    const size_t count =
        std::count_if(uuids.begin(), uuids.end(), [size](const auto &u) { return uuid_size(u) == size; });
    if (count == 0 || this->left_() < 2 + size)
      return;
    size_t fit = std::min(count, (this->left_() - 2) / size);
    this->buf_[this->len_++] = 1 + fit * size;
    this->buf_[this->len_++] = fit < count ? partial_type : partial_type + 1;
    for (const auto &uuid : uuids) {
      if (fit == 0)
        break;
      if (uuid_size(uuid) == size) {
        memcpy(this->buf_.data() + this->len_, uuid.data() + 12, size);
        this->len_ += size;
        fit--;
      }
    }
  }

  std::array<uint8_t, MAX_LEN> buf_{};
  uint8_t len_{0};
};

}  // namespace esphome::esp32_ble

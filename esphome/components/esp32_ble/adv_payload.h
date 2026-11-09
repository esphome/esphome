#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "esphome/core/helpers.h"

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
    uint8_t value[2];
    put_le16(value, appearance);
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
    auto it = std::find_if(uuids.begin(), uuids.end(), [](const auto &u) { return uuid_size(u) == 16; });
    if (it != uuids.end())
      this->add_(0x07, it->data(), 16);
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

/// What the services advertising carries; with `priority`, uuids[0] is the priority UUID.
struct AdvContent {
  std::span<const AdvPayload::UUID128> uuids;
  std::span<const uint8_t> manufacturer_data;
  std::span<const uint8_t> service_data;
  const char *name;
  size_t name_len;
  uint16_t appearance;
  int8_t tx_power_dbm;
  bool name_in_adv;
  bool priority;
};

/// Split `content` between the advertising packet and the scan response. With a priority UUID, appearance,
/// manufacturer data and the other UUIDs move to the scan response, and the service data stays only in `adv`.
inline void build_adv_packets(const AdvContent &content, AdvPayload &adv, AdvPayload &rsp) {
  AdvPayload &extras = content.priority ? rsp : adv;
  adv.add_flags(0x06);  // LE general discoverable, BR/EDR not supported
  extras.add_appearance(content.appearance);
  if (content.name_in_adv)
    adv.add_name(content.name, content.name_len);
  rsp.add_name(content.name, content.name_len);
  extras.add_manufacturer_data(content.manufacturer_data);
  rsp.add_tx_power(content.tx_power_dbm);
  adv.add_uuids(content.priority ? content.uuids.first(1) : content.uuids);
  rsp.add_uuids(content.priority ? content.uuids.subspan(1) : content.uuids);
  adv.add_service_data(content.service_data);
  if (!content.priority)
    rsp.add_service_data(content.service_data);
}

}  // namespace esphome::esp32_ble

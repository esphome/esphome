#pragma once

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

namespace esphome::xiaomi_ble {

struct XiaomiParseResult {
  enum {
    TYPE_HHCCJCY01,
    TYPE_GCLS002,
    TYPE_HHCCPOT002,
    TYPE_LYWSDCGQ,
    TYPE_LYWSD02,
    TYPE_LYWSD02MMC,
    TYPE_CGG1,
    TYPE_LYWSD03MMC,
    TYPE_XMWSDJ04MMC,
    TYPE_CGD1,
    TYPE_CGDK2,
    TYPE_JQJCY01YM,
    TYPE_MUE4094RT,
    TYPE_WX08ZM,
    TYPE_MJYD02YLA,
    TYPE_MHOC303,
    TYPE_MHOC401,
    TYPE_CGPR1,
    TYPE_RTCGQ02LM,
  } type;
  const char *name;
  optional<float> temperature;
  optional<float> humidity;
  optional<float> moisture;
  optional<float> conductivity;
  optional<float> illuminance;
  optional<float> formaldehyde;
  optional<float> battery_level;
  optional<float> tablet;
  optional<float> idle_time;
  optional<bool> is_active;
  optional<bool> has_motion;
  optional<bool> is_light;
  optional<bool> button_press;
  bool has_data;        // 0x40
  bool has_capability;  // 0x20
  bool has_encryption;  // 0x08
  int raw_offset;
};

struct XiaomiAESVector {
  uint8_t key[16];
  uint8_t plaintext[16];
  uint8_t ciphertext[16];
  uint8_t authdata[16];
  uint8_t iv[16];
  uint8_t tag[16];
  size_t keysize;
  size_t authsize;
  size_t datasize;
  size_t tagsize;
  size_t ivsize;
};

/// Largest MiBeacon frame decrypt_xiaomi_payload() accepts.
static constexpr size_t XIAOMI_MAX_ENCRYPTED_FRAME_SIZE = 24;

bool parse_xiaomi_value(uint16_t value_type, const uint8_t *data, uint8_t value_length, XiaomiParseResult &result);
bool parse_xiaomi_message(const uint8_t *message, size_t size, XiaomiParseResult &result);
/// Parses the frame control and product id; the caller checks the 0xFE95 UUID and filters duplicates.
optional<XiaomiParseResult> parse_xiaomi_header(const uint8_t *raw, size_t size);
/// Decrypts the payload of `raw` in place and clears its encryption flag.
bool decrypt_xiaomi_payload(uint8_t *raw, size_t size, const uint8_t *bindkey, uint64_t address);
void report_xiaomi_results(const XiaomiParseResult &result, const char *address);

/// A Xiaomi MiBeacon device: matches its address, decodes each frame once and hands it to publish_result_().
class XiaomiDevice : public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { this->address_ = address; }
  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
    return this->parse_xiaomi_device_(device, nullptr);
  }

 protected:
  /// Encrypted frames are dropped when `bindkey` is null.
  bool parse_xiaomi_device_(const ble_device_base::ESPBTDevice &device, const uint8_t *bindkey);
  virtual void publish_result_(const XiaomiParseResult &result) = 0;

  uint64_t address_{0};
  optional<uint8_t> last_frame_count_;
};

/// A XiaomiDevice whose frames are encrypted with a bindkey.
class XiaomiEncryptedDevice : public XiaomiDevice {
 public:
  void set_bindkey(const char *bindkey);
  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
    return this->parse_xiaomi_device_(device, this->bindkey_);
  }

 protected:
  uint8_t bindkey_[16]{};
};

class XiaomiListener final : public ble_device_base::ESPBTDeviceListener {
 public:
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
};

}  // namespace esphome::xiaomi_ble

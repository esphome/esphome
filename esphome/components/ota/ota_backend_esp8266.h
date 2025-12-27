#pragma once
#ifdef USE_ESP8266
#include "ota_backend.h"

#include "esphome/components/md5/md5.h"
#include "esphome/core/defines.h"

#include <memory>

namespace esphome::ota {

/// OTA backend for ESP8266 using native SDK functions.
/// This implementation bypasses the Arduino Updater library to save ~228 bytes of RAM
/// by not having a global Update object in .bss.
class ESP8266OTABackend : public OTABackend {
 public:
  OTAResponseTypes begin(size_t image_size) override;
  void set_update_md5(const char *md5) override;
  OTAResponseTypes write(uint8_t *data, size_t len) override;
  OTAResponseTypes end() override;
  void abort() override;
  // Compression supported in all ESP8266 Arduino versions ESPHome supports (>= 2.7.0)
  bool supports_compression() override { return true; }

 protected:
  /// Erase flash sector if current address is at sector boundary
  bool erase_sector_if_needed_();

  /// Write buffer to flash (does not update address or clear buffer)
  bool flash_write_();

  /// Write buffered data to flash and update MD5
  bool write_buffer_();

  /// Write buffered data to flash without MD5 update (for final padded write)
  bool write_buffer_final_();

  /// Verify the firmware header is valid
  bool verify_end_();

  /// Get current flash chip mode from flash header
  uint8_t get_flash_chip_mode_();

  std::unique_ptr<uint8_t[]> buffer_;
  size_t buffer_size_{0};
  size_t buffer_len_{0};

  uint32_t start_address_{0};
  uint32_t current_address_{0};
  size_t image_size_{0};

  md5::MD5Digest md5_{};
  uint8_t expected_md5_[16];  // Fixed-size buffer for 128-bit (16-byte) MD5 digest
  bool md5_set_{false};
};

}  // namespace esphome::ota
#endif  // USE_ESP8266

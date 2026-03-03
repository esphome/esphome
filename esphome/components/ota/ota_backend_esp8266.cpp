#ifdef USE_ESP8266
#include "ota_backend_esp8266.h"
#include "ota_backend.h"

#include "esphome/components/esp8266/preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <Esp.h>
#include <esp8266_peri.h>

#include <cinttypes>

extern "C" {
#include <c_types.h>
#include <eboot_command.h>
#include <flash_hal.h>
#include <spi_flash.h>
#include <user_interface.h>
}

// Note: FLASH_SECTOR_SIZE (0x1000) is already defined in spi_flash_geometry.h

// Flash header offsets
static constexpr uint8_t FLASH_MODE_OFFSET = 2;

// Firmware magic bytes
static constexpr uint8_t FIRMWARE_MAGIC = 0xE9;
static constexpr uint8_t GZIP_MAGIC_1 = 0x1F;
static constexpr uint8_t GZIP_MAGIC_2 = 0x8B;

// ESP8266 flash memory base address (memory-mapped flash starts here)
static constexpr uint32_t FLASH_BASE_ADDRESS = 0x40200000;

// Boot mode extraction from GPI register (bits 16-19 contain boot mode)
static constexpr int BOOT_MODE_SHIFT = 16;
static constexpr int BOOT_MODE_MASK = 0xf;

// Boot mode indicating UART download mode (OTA not possible)
static constexpr int BOOT_MODE_UART_DOWNLOAD = 1;

// Minimum buffer size when memory is constrained
static constexpr size_t MIN_BUFFER_SIZE = 256;

namespace esphome::ota {

static const char *const TAG = "ota.esp8266";

std::unique_ptr<ota::OTABackend> make_ota_backend() { return make_unique<ota::ESP8266OTABackend>(); }

OTAResponseTypes ESP8266OTABackend::begin(size_t image_size) {
  // Handle UPDATE_SIZE_UNKNOWN (0) by calculating available space
  if (image_size == 0) {
    // Round down to sector boundary: subtract one sector, then mask to sector alignment
    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    image_size = (ESP.getFreeSketchSpace() - FLASH_SECTOR_SIZE) & ~(FLASH_SECTOR_SIZE - 1);
  }

  // Check boot mode - if boot mode is UART download mode,
  // we will not be able to reset into normal mode once update is done
  int boot_mode = (GPI >> BOOT_MODE_SHIFT) & BOOT_MODE_MASK;
  if (boot_mode == BOOT_MODE_UART_DOWNLOAD) {
    return OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING;
  }

  // Check flash configuration - real size must be >= configured size
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  if (!ESP.checkFlashConfig(false)) {
    return OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG;
  }

  // Get current sketch size
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  uint32_t sketch_size = ESP.getSketchSize();

  // Size of current sketch rounded to sector boundary
  uint32_t current_sketch_size = (sketch_size + FLASH_SECTOR_SIZE - 1) & (~(FLASH_SECTOR_SIZE - 1));

  // Size of update rounded to sector boundary
  uint32_t rounded_size = (image_size + FLASH_SECTOR_SIZE - 1) & (~(FLASH_SECTOR_SIZE - 1));

  // End of available space for sketch and update (start of filesystem)
  uint32_t update_end_address = FS_start - FLASH_BASE_ADDRESS;

  // Calculate start address for the update (write from end backwards)
  this->start_address_ = (update_end_address > rounded_size) ? (update_end_address - rounded_size) : 0;

  // Check if there's enough space for both current sketch and update
  if (this->start_address_ < current_sketch_size) {
    return OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE;
  }

  // Allocate buffer for sector writes (use smaller buffer if memory constrained)
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  this->buffer_size_ = (ESP.getFreeHeap() > 2 * FLASH_SECTOR_SIZE) ? FLASH_SECTOR_SIZE : MIN_BUFFER_SIZE;

  // ESP8266's umm_malloc guarantees 4-byte aligned allocations, which is required
  // for spi_flash_write(). This is the same pattern used by Arduino's Updater class.
  this->buffer_ = make_unique<uint8_t[]>(this->buffer_size_);
  if (!this->buffer_) {
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  this->current_address_ = this->start_address_;
  this->image_size_ = image_size;
  this->buffer_len_ = 0;
  this->md5_set_ = false;

  // Disable WiFi sleep during update
  wifi_set_sleep_type(NONE_SLEEP_T);

  // Prevent preference writes during update
  esp8266::preferences_prevent_write(true);

  // Initialize MD5 computation
  this->md5_.init();

  ESP_LOGD(TAG, "OTA begin: start=0x%08" PRIX32 ", size=%zu", this->start_address_, image_size);

  return OTA_RESPONSE_OK;
}

void ESP8266OTABackend::set_update_md5(const char *md5) {
  // Parse hex string to bytes
  if (parse_hex(md5, this->expected_md5_, 16)) {
    this->md5_set_ = true;
  }
}

OTAResponseTypes ESP8266OTABackend::write(uint8_t *data, size_t len) {
  if (!this->buffer_) {
    return OTA_RESPONSE_ERROR_UNKNOWN;
  }

  size_t written = 0;
  while (written < len) {
    // Calculate how much we can buffer
    size_t to_buffer = std::min(len - written, this->buffer_size_ - this->buffer_len_);
    memcpy(this->buffer_.get() + this->buffer_len_, data + written, to_buffer);
    this->buffer_len_ += to_buffer;
    written += to_buffer;

    // If buffer is full, write to flash
    if (this->buffer_len_ == this->buffer_size_ && !this->write_buffer_()) {
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
  }

  return OTA_RESPONSE_OK;
}

bool ESP8266OTABackend::erase_sector_if_needed_() {
  if ((this->current_address_ % FLASH_SECTOR_SIZE) != 0) {
    return true;  // Not at sector boundary
  }

  App.feed_wdt();
  if (spi_flash_erase_sector(this->current_address_ / FLASH_SECTOR_SIZE) != SPI_FLASH_RESULT_OK) {
    ESP_LOGE(TAG, "Flash erase failed at 0x%08" PRIX32, this->current_address_);
    return false;
  }
  return true;
}

bool ESP8266OTABackend::flash_write_() {
  App.feed_wdt();
  if (spi_flash_write(this->current_address_, reinterpret_cast<uint32_t *>(this->buffer_.get()), this->buffer_len_) !=
      SPI_FLASH_RESULT_OK) {
    ESP_LOGE(TAG, "Flash write failed at 0x%08" PRIX32, this->current_address_);
    return false;
  }
  return true;
}

bool ESP8266OTABackend::write_buffer_() {
  if (this->buffer_len_ == 0) {
    return true;
  }

  if (!this->erase_sector_if_needed_()) {
    return false;
  }

  // Patch flash mode in first sector if needed
  // This is analogous to what esptool.py does when it receives a --flash_mode argument
  bool is_first_sector = (this->current_address_ == this->start_address_);
  uint8_t original_flash_mode = 0;
  bool patched_flash_mode = false;

  // Only patch if we have enough bytes to access flash mode offset and it's not GZIP
  if (is_first_sector && this->buffer_len_ > FLASH_MODE_OFFSET && this->buffer_[0] != GZIP_MAGIC_1) {
    // Not GZIP compressed - check and patch flash mode
    uint8_t current_flash_mode = this->get_flash_chip_mode_();
    uint8_t buffer_flash_mode = this->buffer_[FLASH_MODE_OFFSET];

    if (buffer_flash_mode != current_flash_mode) {
      original_flash_mode = buffer_flash_mode;
      this->buffer_[FLASH_MODE_OFFSET] = current_flash_mode;
      patched_flash_mode = true;
    }
  }

  if (!this->flash_write_()) {
    return false;
  }

  // Restore original flash mode for MD5 calculation
  if (patched_flash_mode) {
    this->buffer_[FLASH_MODE_OFFSET] = original_flash_mode;
  }

  // Update MD5 with original (unpatched) data
  this->md5_.add(this->buffer_.get(), this->buffer_len_);

  this->current_address_ += this->buffer_len_;
  this->buffer_len_ = 0;

  return true;
}

bool ESP8266OTABackend::write_buffer_final_() {
  // Similar to write_buffer_(), but without flash mode patching or MD5 update (for final padded write)
  if (this->buffer_len_ == 0) {
    return true;
  }

  if (!this->erase_sector_if_needed_() || !this->flash_write_()) {
    return false;
  }

  this->current_address_ += this->buffer_len_;
  this->buffer_len_ = 0;

  return true;
}

OTAResponseTypes ESP8266OTABackend::end() {
  // Write any remaining buffered data
  if (this->buffer_len_ > 0) {
    // Add actual data to MD5 before padding
    this->md5_.add(this->buffer_.get(), this->buffer_len_);

    // Pad to 4-byte alignment for flash write
    while (this->buffer_len_ % 4 != 0) {
      this->buffer_[this->buffer_len_++] = 0xFF;
    }
    if (!this->write_buffer_final_()) {
      this->abort();
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
  }

  // Calculate actual bytes written
  size_t actual_size = this->current_address_ - this->start_address_;

  // Check if any data was written
  if (actual_size == 0) {
    ESP_LOGE(TAG, "No data written");
    this->abort();
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  // Verify MD5 if set (strict mode), otherwise use lenient mode
  // In lenient mode (no MD5), we accept whatever was written
  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_bytes(this->expected_md5_)) {
      ESP_LOGE(TAG, "MD5 mismatch");
      this->abort();
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  } else {
    // Lenient mode: adjust size to what was actually written
    // This matches Arduino's Update.end(true) behavior
    this->image_size_ = actual_size;
  }

  // Verify firmware header
  if (!this->verify_end_()) {
    this->abort();
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  // Write eboot command to copy firmware on next boot
  eboot_command ebcmd;
  ebcmd.action = ACTION_COPY_RAW;
  ebcmd.args[0] = this->start_address_;
  ebcmd.args[1] = 0x00000;  // Destination: start of flash
  ebcmd.args[2] = this->image_size_;
  eboot_command_write(&ebcmd);

  ESP_LOGI(TAG, "OTA update staged: 0x%08" PRIX32 " -> 0x00000, size=%zu", this->start_address_, this->image_size_);

  // Clean up
  this->buffer_.reset();
  esp8266::preferences_prevent_write(false);

  return OTA_RESPONSE_OK;
}

void ESP8266OTABackend::abort() {
  this->buffer_.reset();
  this->buffer_len_ = 0;
  this->image_size_ = 0;
  esp8266::preferences_prevent_write(false);
}

bool ESP8266OTABackend::verify_end_() {
  uint32_t buf;
  if (spi_flash_read(this->start_address_, &buf, 4) != SPI_FLASH_RESULT_OK) {
    ESP_LOGE(TAG, "Failed to read firmware header");
    return false;
  }

  uint8_t *bytes = reinterpret_cast<uint8_t *>(&buf);

  // Check for GZIP (compressed firmware)
  if (bytes[0] == GZIP_MAGIC_1 && bytes[1] == GZIP_MAGIC_2) {
    // GZIP compressed - can't verify further
    return true;
  }

  // Check firmware magic byte
  if (bytes[0] != FIRMWARE_MAGIC) {
    ESP_LOGE(TAG, "Invalid firmware magic: 0x%02X (expected 0x%02X)", bytes[0], FIRMWARE_MAGIC);
    return false;
  }

#if !FLASH_MAP_SUPPORT
  // Check if new firmware's flash size fits (only when auto-detection is disabled)
  // With FLASH_MAP_SUPPORT (modern cores), flash size is auto-detected from chip
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  uint32_t bin_flash_size = ESP.magicFlashChipSize((bytes[3] & 0xf0) >> 4);
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  if (bin_flash_size > ESP.getFlashChipRealSize()) {
    ESP_LOGE(TAG, "Firmware flash size (%" PRIu32 ") exceeds chip size (%" PRIu32 ")", bin_flash_size,
             ESP.getFlashChipRealSize());
    return false;
  }
#endif

  return true;
}

uint8_t ESP8266OTABackend::get_flash_chip_mode_() {
  uint32_t data;
  if (spi_flash_read(0x0000, &data, 4) != SPI_FLASH_RESULT_OK) {
    return 0;  // Default to QIO
  }
  return (reinterpret_cast<uint8_t *>(&data))[FLASH_MODE_OFFSET];
}

}  // namespace esphome::ota
#endif  // USE_ESP8266

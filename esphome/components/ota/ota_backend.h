#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace ota {

enum OTAResponseTypes {
  OTA_RESPONSE_OK = 0,
  OTA_RESPONSE_REQUEST_AUTH = 1,
  OTA_RESPONSE_REQUEST_MD5 = 2,

  OTA_RESPONSE_HEADER_OK = 64,
  OTA_RESPONSE_AUTH_OK = 65,
  OTA_RESPONSE_UPDATE_PREPARE_OK = 66,
  OTA_RESPONSE_BIN_MD5_OK = 67,
  OTA_RESPONSE_RECEIVE_OK = 68,
  OTA_RESPONSE_UPDATE_END_OK = 69,
  OTA_RESPONSE_SUPPORTS_COMPRESSION = 70,
  OTA_RESPONSE_SUPPORTS_EXTENDED = 71,
  OTA_RESPONSE_READ_PREPARE_OK = 72,
  OTA_RESPONSE_READ_END_OK = 73,

  OTA_RESPONSE_ERROR_MAGIC = 128,
  OTA_RESPONSE_ERROR_UPDATE_PREPARE = 129,
  OTA_RESPONSE_ERROR_AUTH_INVALID = 130,
  OTA_RESPONSE_ERROR_WRITING_FLASH = 131,
  OTA_RESPONSE_ERROR_UPDATE_END = 132,
  OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING = 133,
  OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG = 134,
  OTA_RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG = 135,
  OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE = 136,
  OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE = 137,
  OTA_RESPONSE_ERROR_NO_UPDATE_PARTITION = 138,
  OTA_RESPONSE_ERROR_MD5_MISMATCH = 139,
  OTA_RESPONSE_ERROR_RP2040_NOT_ENOUGH_SPACE = 140,
  OTA_RESPONSE_ERROR_UNKNOWN_PARTITION_INFO_VERSION = 141,
  OTA_RESPONSE_ERROR_BIN_TYPE_NOT_SUPPORTED = 142,
  OTA_RESPONSE_ERROR_ESP32_REGISTERING_PARTITION = 143,
  OTA_RESPONSE_ERROR_PARTITION_NOT_FOUND = 144,
  OTA_RESPONSE_ERROR_UNKNOWN_COMMAND = 145,
  OTA_RESPONSE_ERROR_ABORT_OVERRIDE = 146,
  OTA_RESPONSE_ERROR_SOCKET_READ = 147,
  OTA_RESPONSE_ERROR_READ_NOT_SUPPORTED = 148,
  OTA_RESPONSE_ERROR_READING_FLASH = 149,
  OTA_RESPONSE_ERROR_UNKNOWN = 255,
};

enum OTABinType {
  OTA_BIN_APP = 1,
  OTA_BIN_BOOTLOADER = 2,
  OTA_BIN_PARTITION_TABLE = 3,
  OTA_BIN_PARTITION = 4,
};

struct OTAPartitionType {
  OTABinType type;
  uint8_t part_type;
  uint8_t part_subtype;
  uint8_t part_index;
  char part_label[16];
};

class OTABackend {
 public:
  virtual ~OTABackend() = default;
  virtual OTAResponseTypes begin(OTAPartitionType bin_type, size_t &image_size) = 0;
  virtual void set_update_md5(const char *md5) = 0;
  virtual OTAResponseTypes write(uint8_t *data, size_t len) = 0;
  virtual OTAResponseTypes read(uint8_t *data, size_t len) { return OTA_RESPONSE_ERROR_READ_NOT_SUPPORTED; }
  virtual OTAResponseTypes end() = 0;
  virtual void abort() = 0;
  virtual bool supports_compression() = 0;
  virtual bool supports_writing_bootloader() { return false; }
  virtual bool supports_writing_partition_table() { return false; }
  virtual bool supports_writing_partitions() { return false; }
  virtual bool supports_reading() { return false; }
};

}  // namespace ota
}  // namespace esphome

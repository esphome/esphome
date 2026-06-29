#pragma once

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)

#include <cstdint>

namespace esphome {
namespace usb_storage {

// USB MSC Bulk-Only Transport constants
static constexpr uint32_t MSC_BOT_CBW_SIGNATURE = 0x43425355;  // "USBC"
static constexpr uint32_t MSC_BOT_CSW_SIGNATURE = 0x53425355;  // "USBS"
static constexpr uint8_t MSC_BOT_CBW_FLAGS_IN = 0x80;
static constexpr uint8_t MSC_BOT_CBW_FLAGS_OUT = 0x00;
static constexpr uint8_t MSC_BOT_CSW_STATUS_GOOD = 0x00;
static constexpr uint8_t MSC_BOT_CSW_STATUS_FAILED = 0x01;
static constexpr uint8_t MSC_BOT_CSW_STATUS_PHASE_ERROR = 0x02;

// MSC class-specific requests
static constexpr uint8_t MSC_REQUEST_RESET = 0xFF;
static constexpr uint8_t MSC_REQUEST_GET_MAX_LUN = 0xFE;

// Command Block Wrapper (31 bytes, little-endian)
struct __attribute__((packed)) MscCbw {
  uint32_t signature{MSC_BOT_CBW_SIGNATURE};
  uint32_t tag{0};
  uint32_t data_transfer_length{0};
  uint8_t flags{0};   // 0x80 = IN (device→host), 0x00 = OUT (host→device)
  uint8_t lun{0};
  uint8_t cb_length{0};
  uint8_t cb[16]{};
};
static_assert(sizeof(MscCbw) == 31, "MscCbw must be 31 bytes");

// Command Status Wrapper (13 bytes, little-endian)
struct __attribute__((packed)) MscCsw {
  uint32_t signature{0};
  uint32_t tag{0};
  uint32_t data_residue{0};
  uint8_t status{0};
};
static_assert(sizeof(MscCsw) == 13, "MscCsw must be 13 bytes");

// SCSI command opcodes
static constexpr uint8_t SCSI_CMD_TEST_UNIT_READY = 0x00;
static constexpr uint8_t SCSI_CMD_INQUIRY = 0x12;
static constexpr uint8_t SCSI_CMD_READ_CAPACITY_10 = 0x25;
static constexpr uint8_t SCSI_CMD_READ_10 = 0x28;
static constexpr uint8_t SCSI_CMD_WRITE_10 = 0x2A;
static constexpr uint8_t SCSI_CMD_REQUEST_SENSE = 0x03;

// INQUIRY response (36 bytes minimum)
struct __attribute__((packed)) ScsiInquiryResponse {
  uint8_t peripheral_qualifier_type;  // bits 7:5 = qualifier, bits 4:0 = device type (0=direct access)
  uint8_t removable;                  // bit 7 = removable
  uint8_t version;
  uint8_t response_data_format;
  uint8_t additional_length;
  uint8_t reserved[3];
  char vendor_id[8];
  char product_id[16];
  char product_rev[4];
};
static_assert(sizeof(ScsiInquiryResponse) == 36, "ScsiInquiryResponse must be 36 bytes");

// READ CAPACITY (10) response (8 bytes)
struct __attribute__((packed)) ScsiReadCapacity10Response {
  uint32_t last_lba;    // big-endian: last addressable LBA
  uint32_t block_size;  // big-endian: bytes per block
};
static_assert(sizeof(ScsiReadCapacity10Response) == 8, "ScsiReadCapacity10Response must be 8 bytes");

}  // namespace usb_storage
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4

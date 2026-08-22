#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BINARY_STORAGE_SPI_FLASH

#include "binary_storage.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP_IDF
#include <vector>
extern "C" {
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
}
#endif

namespace esphome::binary_storage {

/**
 * @brief SPI NOR Flash storage device (W25Q, MX25, AT25 series)
 *
 * Standard SPI flash memory with:
 * - Page program (256 bytes)
 * - Sector erase (4KB minimum)
 * - Block erase (32KB, 64KB)
 * - ~100,000 erase cycles
 * - JEDEC ID for auto-detection
 *
 * Supports standard JEDEC-compatible flash chips
 */
class SPIFlash : public BinaryStorage,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_8MHZ> {
 public:
  SPIFlash() = default;

  // Component lifecycle
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override {
#ifdef USE_ESP_IDF
    // In esp_partition mode the regions become real partitions that later consumers (e.g. an
    // external NVS backing preferences) resolve by label. Register them at IO -- before the
    // DATA-priority default where most preference-reading components set up -- so the partitions
    // exist by the time those components first touch them.
    if (this->esp_partition_mode_)
      return setup_priority::IO;
#endif
    return setup_priority::DATA;
  }

  //========================================================================
  // Configuration
  //========================================================================

  /**
   * @brief Set flash model/type
   *
   * @param model Model string (e.g., "W25Q32", "MX25L128")
   */
  void set_model(const std::string &model) { this->model_ = model; }

  /**
   * @brief Set capacity in bytes (if not auto-detected)
   *
   * @param capacity Capacity in bytes
   */
  void set_capacity(uint32_t capacity) { this->capacity_ = capacity; }

  /**
   * @brief Set page size (default: 256 bytes)
   *
   * @param page_size Page size in bytes
   */
  void set_page_size(uint32_t page_size) { this->page_size_ = page_size; }

  /**
   * @brief Set sector size (default: 4096 bytes)
   *
   * @param sector_size Sector size in bytes
   */
  void set_sector_size(uint32_t sector_size) { this->sector_size_ = sector_size; }

  /**
   * @brief Enable/disable Quad SPI mode for faster reads
   *
   * Quad SPI uses 4 data lines instead of 1, providing 4x faster read performance.
   * Most modern flash chips support this (W25Q, MX25, etc.)
   *
   * @param enable true to enable quad mode, false for standard SPI
   */
  void set_quad_mode(bool enable) { this->quad_mode_ = enable; }

#ifdef USE_ESP_IDF
  // esp_partition mode: drive the flash with the IDF esp_flash driver on a dedicated (exclusive)
  // SPI bus and register its regions as real esp_partitions. Enabled by codegen only on
  // esp32/esp-idf when the bus is exclusive; the flash is NOT an ESPHome spi_device in this mode.
  struct PartitionRegion {
    uint64_t offset;
    uint64_t size;
    const char *label;
    uint8_t subtype;  // esp_partition_subtype_t
  };
  void enable_esp_partition_mode(uint8_t spi_host, int cs_pin, int freq_mhz) {
    this->esp_partition_mode_ = true;
    this->spi_host_ = spi_host;
    this->cs_pin_num_ = cs_pin;
    this->flash_freq_mhz_ = freq_mhz;
  }
  void add_partition_region(uint64_t offset, uint64_t size, const char *label, uint8_t subtype) {
    this->partition_regions_.push_back(PartitionRegion{offset, size, label, subtype});
  }
#endif

  //========================================================================
  // BinaryStorage Interface
  //========================================================================

  uint32_t get_capacity() const override { return this->capacity_; }
  const char *get_device_name() const override { return this->model_.c_str(); }
  const char *get_device_type() const override { return "spi_flash"; }
  uint32_t get_page_size() const override { return this->page_size_; }
  uint32_t get_erase_size() const override { return this->sector_size_; }
  uint32_t get_erase_block_size() const override { return BLOCK_SIZE_64K; }
  // NOR flash: bits only clear on write, so every write needs the covering unit erased first.
  // All three granularities are standard SPI NOR opcodes and already implemented below.
  uint8_t get_erase_caps() const override {
    return storage::RAW_WRITE_NEEDS_ERASE | storage::RAW_ERASE_SECTOR | storage::RAW_ERASE_BLOCK |
           storage::RAW_ERASE_CHIP;
  }

  // RawStorage interface
  storage::StorageError read_physical(uint64_t offset, uint8_t *buf, size_t len, size_t *bytes_transferred) override;
  storage::StorageError write_physical(uint64_t offset, const uint8_t *buf, size_t len,
                                       size_t *bytes_transferred) override;
  storage::StorageError erase_physical(uint64_t offset, size_t len) override;
  storage::StorageError format() override;

  // Hardware-level byte access
  bool is_ready() override;
  bool read_raw(uint32_t address, uint8_t *data, size_t length);
  bool write_raw(uint32_t address, const uint8_t *data, size_t length);
  bool erase_block(uint32_t address);

  //========================================================================
  // Flash-Specific Operations
  //========================================================================

  /**
   * @brief Read JEDEC ID (manufacturer + device ID)
   *
   * @return 24-bit JEDEC ID (manufacturer: bits 23-16, device: bits 15-0)
   */
  uint32_t read_jedec_id();

  /**
   * @brief Get manufacturer ID from JEDEC ID
   *
   * @return Manufacturer ID (0xEF=Winbond, 0xC2=Macronix, 0x1F=Adesto, etc.)
   */
  uint8_t get_manufacturer_id() const { return this->jedec_id_ >> 16; }

  /**
   * @brief Get device ID from JEDEC ID
   *
   * @return 16-bit device ID
   */
  uint16_t get_device_id() const { return this->jedec_id_ & 0xFFFF; }

  /**
   * @brief Erase a 4KB sector
   *
   * @param address Any address within the sector
   * @return true on success
   */
  bool erase_sector(uint32_t address);

  /**
   * @brief Erase a 32KB block
   *
   * @param address Any address within the block
   * @return true on success
   */
  bool erase_block_32k(uint32_t address);

  /**
   * @brief Erase a 64KB block
   *
   * @param address Any address within the block
   * @return true on success
   */
  bool erase_block_64k(uint32_t address);

  /**
   * @brief Erase entire chip
   *
   * WARNING: This erases ALL data!
   *
   * @return true on success
   */
  bool erase_chip();

  /**
   * @brief Power down flash (ultra-low power mode)
   */
  void power_down();

  /**
   * @brief Wake up from power down
   */
  void wake_up();

  //========================================================================
  // Status Register & Write Protection
  //========================================================================

  /**
   * @brief Read status register 1
   *
   * @return Status register 1 value
   */
  uint8_t read_status_register();

  /**
   * @brief Read status register 2 (for quad enable bit)
   *
   * @return Status register 2 value
   */
  uint8_t read_status_register2();

  /**
   * @brief Write status registers (SR1 + SR2)
   *
   * Writes both status registers together (required by Winbond/Macronix).
   * Automatically handles write enable and wait for completion.
   *
   * @param sr1 Status register 1 value
   * @param sr2 Status register 2 value
   * @return true on success
   */
  bool write_status_registers(uint8_t sr1, uint8_t sr2);

  /**
   * @brief Wait for flash to become ready (not busy)
   *
   * @param timeout_ms Maximum time to wait (default 5000ms)
   * @return true if ready, false if timeout
   */
  bool wait_ready(uint32_t timeout_ms = 5000);

  /**
   * @brief Sync -- waits for any in-progress operation to complete
   *
   * @return true if device is ready
   */
  bool sync();

  /**
   * @brief Enable writes (must be called before any write/erase operation)
   *
   * @return true on success
   */
  bool write_enable();

  /**
   * @brief Disable writes
   */
  void write_disable();

 protected:
  //========================================================================
  // Device Configuration
  //========================================================================

  std::string model_{"SPI_FLASH"};
  uint32_t capacity_{0};
  uint32_t page_size_{256};     // Standard page size
  uint32_t sector_size_{4096};  // Standard sector size (4KB)
  uint32_t jedec_id_{0};
  bool quad_mode_{false};       // Quad SPI mode for faster reads
  bool four_byte_mode_{false};  // 4-byte addressing for chips > 16MB

#ifdef USE_ESP_IDF
  bool esp_partition_mode_{false};
  uint8_t spi_host_{0};
  int cs_pin_num_{-1};
  int flash_freq_mhz_{40};
  esp_flash_t *ext_chip_{nullptr};
  std::vector<PartitionRegion> partition_regions_;
  bool esp_partition_setup_();  // esp_flash init + register the region partitions; false on failure
#endif

  //========================================================================
  // SPI Flash Commands (JEDEC standard)
  //========================================================================

  static constexpr uint8_t CMD_WRITE_ENABLE = 0x06;
  static constexpr uint8_t CMD_WRITE_DISABLE = 0x04;
  static constexpr uint8_t CMD_READ_STATUS_REG1 = 0x05;
  static constexpr uint8_t CMD_READ_STATUS_REG2 = 0x35;
  static constexpr uint8_t CMD_WRITE_STATUS_REG = 0x01;
  static constexpr uint8_t CMD_PAGE_PROGRAM = 0x02;
  static constexpr uint8_t CMD_QUAD_PAGE_PROGRAM = 0x32;
  // Erase unit sizes the 32K/64K block opcodes below operate on.
  static constexpr uint32_t BLOCK_SIZE_32K = 32768;
  static constexpr uint32_t BLOCK_SIZE_64K = 65536;

  static constexpr uint8_t CMD_BLOCK_ERASE_64K = 0xD8;
  static constexpr uint8_t CMD_BLOCK_ERASE_32K = 0x52;
  static constexpr uint8_t CMD_SECTOR_ERASE_4K = 0x20;
  static constexpr uint8_t CMD_CHIP_ERASE = 0xC7;
  static constexpr uint8_t CMD_ERASE_SUSPEND = 0x75;
  static constexpr uint8_t CMD_ERASE_RESUME = 0x7A;
  static constexpr uint8_t CMD_POWER_DOWN = 0xB9;
  static constexpr uint8_t CMD_RELEASE_POWER_DOWN = 0xAB;
  static constexpr uint8_t CMD_MANUFACTURER_DEVICE_ID = 0x90;
  static constexpr uint8_t CMD_JEDEC_ID = 0x9F;
  static constexpr uint8_t CMD_READ_BYTES = 0x03;
  static constexpr uint8_t CMD_FAST_READ = 0x0B;
  static constexpr uint8_t CMD_READ_UNIQUE_ID = 0x4B;

  // 4-byte address mode commands (for chips > 16MB)
  static constexpr uint8_t CMD_ENTER_4BYTE_ADDR_MODE = 0xB7;
  static constexpr uint8_t CMD_EXIT_4BYTE_ADDR_MODE = 0xE9;
  static constexpr uint8_t CMD_READ_CONFIG_REG = 0x15;  // Macronix: bit 5 (ADP) = 4-byte mode active

  // Quad SPI commands (4x faster reads)
  static constexpr uint8_t CMD_FAST_READ_QUAD_OUTPUT = 0x6B;  // Address on 1 line, data on 4 lines
  static constexpr uint8_t CMD_FAST_READ_QUAD_IO = 0xEB;      // Address and data on 4 lines

  // Status register 1 bits
  static constexpr uint8_t STATUS_BUSY = 0x01;  // Write in progress
  static constexpr uint8_t STATUS_WEL = 0x02;   // Write enable latch
  static constexpr uint8_t STATUS_BP0 = 0x04;   // Block protect bit 0
  static constexpr uint8_t STATUS_BP1 = 0x08;   // Block protect bit 1
  static constexpr uint8_t STATUS_BP2 = 0x10;   // Block protect bit 2
  static constexpr uint8_t STATUS_TB = 0x20;    // Top/bottom protect
  static constexpr uint8_t STATUS_SEC = 0x40;   // Sector protect
  static constexpr uint8_t STATUS_SRP = 0x80;   // Status register protect

  // Status register 2 bits (Winbond/Macronix)
  static constexpr uint8_t STATUS2_QE = 0x02;  // Quad Enable (bit 1 of SR2)

  //========================================================================
  // Internal Helpers
  //========================================================================

  /**
   * @brief Enable quad mode in status register
   *
   * Sets the QE (Quad Enable) bit in status register 2
   *
   * @return true on success
   */
  bool enable_quad_mode_();

  /**
   * @brief Disable quad mode in status register
   *
   * Clears the QE (Quad Enable) bit in status register 2
   *
   * @return true on success
   */
  bool disable_quad_mode_();

  /**
   * @brief Write a page (up to 256 bytes, must not cross page boundary)
   *
   * @param address Starting address (must be page-aligned for best performance)
   * @param data Data to write
   * @param length Number of bytes (max 256, must not cross page boundary)
   * @return true on success
   */
  bool write_page_(uint32_t address, const uint8_t *data, size_t length);

  /**
   * @brief Read data from flash
   *
   * @param address Starting address
   * @param data Buffer for read data
   * @param length Number of bytes to read
   * @return true on success
   */
  bool read_data_(uint32_t address, uint8_t *data, size_t length);

  /**
   * @brief Perform erase operation
   *
   * @param address Address within block/sector
   * @param cmd Erase command (SECTOR_ERASE, BLOCK_ERASE_32K, BLOCK_ERASE_64K)
   * @return true on success
   */
  bool erase_(uint32_t address, uint8_t cmd);

  /**
   * @brief Auto-configure from JEDEC ID
   */
  void auto_configure_from_jedec_id_();

  /**
   * @brief Write address bytes (3 or 4 depending on addressing mode)
   *
   * @param address Address to write
   */
  void write_address_(uint32_t address);

  /**
   * @brief Enter 4-byte address mode for chips > 16MB
   *
   * @return true on success
   */
  bool enter_4byte_mode_();

  /**
   * @brief Exit 4-byte address mode
   *
   * @return true on success
   */
  bool exit_4byte_mode_();

  /**
   * @brief Get number of bytes remaining in current page
   *
   * @param address Current address
   * @return Bytes until page boundary
   */
  uint32_t get_bytes_to_page_boundary_(uint32_t address) const {
    return this->page_size_ - (address % this->page_size_);
  }
};

}  // namespace esphome::binary_storage

#endif  // USE_BINARY_STORAGE_SPI_FLASH

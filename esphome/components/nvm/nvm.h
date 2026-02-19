#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"
#include <vector>
#include <memory>

namespace esphome {
namespace nvm {

/// RTC key for boot loop counter - used to delegate to NVS preferences
/// TODO: Use safe_mode::RTC_KEY once PR #14121 is merged
static const uint32_t RTC_KEY = 233825507UL;

/// Partition types supported by NVM
enum class PartitionType : uint8_t {
  PREFERENCES = 0,  ///< ESPHome preferences backend
  RAW = 1,          ///< Raw byte storage
  KEY_VALUE = 2,    ///< Key-value store
};

/// Convert PartitionType to string for logging
const char *partition_type_to_string(PartitionType type);

/// Partition configuration
struct PartitionConfig {
  std::string id;      ///< User-defined partition ID
  PartitionType type;  ///< Partition type
  uint32_t offset;     ///< Offset in NVM device (auto-calculated if 0)
  uint32_t size;       ///< Size in bytes
};

/// Forward declaration
class NvmPlatform;

/// Base class for NVM partitions
///
/// This class provides a logical view of a portion of an NVM device.
/// Partitions are owned by an NvmPlatform and provide type-specific
/// interfaces for accessing the underlying storage.
class NvmPartition {
 public:
  NvmPartition(NvmPlatform *parent, const PartitionConfig &config);
  virtual ~NvmPartition() = default;

  /// Read data from partition
  /// @param offset Offset within partition (not absolute NVM offset)
  /// @param data Buffer to read into
  /// @param len Number of bytes to read
  /// @return true on success
  bool read(uint32_t offset, uint8_t *data, size_t len);

  /// Write data to partition
  /// @param offset Offset within partition (not absolute NVM offset)
  /// @param data Data to write
  /// @param len Number of bytes to write
  /// @return true on success
  bool write(uint32_t offset, const uint8_t *data, size_t len);

  /// Get partition ID
  const std::string &get_id() const { return config_.id; }

  /// Get partition type
  PartitionType get_type() const { return config_.type; }

  /// Get partition offset (absolute in NVM device)
  uint32_t get_offset() const { return config_.offset; }

  /// Get partition size
  uint32_t get_size() const { return config_.size; }

  /// Get parent NVM platform
  NvmPlatform *get_parent() const { return parent_; }

 protected:
  friend class NvmPlatform;
  NvmPlatform *parent_;
  PartitionConfig config_;
};

/// Abstract base class for NVM platforms (FRAM, EEPROM, etc.)
///
/// This class defines the interface that all NVM platforms must implement.
/// Platforms are responsible for the low-level read/write operations to
/// the physical device. The base class handles partition management.
///
/// Platform implementations:
/// - FramI2cPlatform: I2C FRAM devices (MB85RC series)
/// - FramSpiPlatform: SPI FRAM devices (MB85RS series) - future
/// - EepromI2cPlatform: I2C EEPROM devices (AT24C series) - future
/// - EepromSpiPlatform: SPI EEPROM devices (AT25 series) - future
class NvmPlatform : public Component {
 public:
  NvmPlatform() = default;
  ~NvmPlatform() = default;

  // ========== Abstract interface - must be implemented by platform ==========

  /// Read bytes from NVM device
  /// @param memaddr Absolute memory address
  /// @param data Buffer to read into
  /// @param len Number of bytes to read
  /// @return true on success
  virtual bool read_bytes(uint32_t memaddr, uint8_t *data, size_t len) = 0;

  /// Write bytes to NVM device
  /// @param memaddr Absolute memory address
  /// @param data Data to write
  /// @param len Number of bytes to write
  /// @return true on success
  virtual bool write_bytes(uint32_t memaddr, const uint8_t *data, size_t len) = 0;

  /// Get total size of NVM device in bytes
  virtual uint32_t get_total_size() const = 0;

  // ========== Partition management ==========

  /// Add a partition to this NVM device
  /// @param config Partition configuration
  /// @return Pointer to created partition, or nullptr on failure
  NvmPartition *add_partition(const PartitionConfig &config);

  /// Get partition by ID
  /// @param id Partition ID
  /// @return Pointer to partition, or nullptr if not found
  NvmPartition *get_partition(const std::string &id);

  /// Get all partitions
  const std::vector<std::unique_ptr<NvmPartition>> &get_partitions() const { return partitions_; }

  /// Validate partition configuration
  /// @param config Partition configuration to validate
  /// @return true if valid
  bool validate_partition_config(const PartitionConfig &config);

  // ========== Component methods ==========
  void setup() override;
  void dump_config() override;

 protected:
  std::vector<std::unique_ptr<NvmPartition>> partitions_;

  /// Check for partition overlaps
  bool check_partition_overlap(const PartitionConfig &config);

  /// Calculate automatic offsets for partitions
  void calculate_partition_offsets();
};

/// Base class for data partitions (Preferences and KeyValue)
///
/// This class provides common functionality for both partition types,
/// including header management, validation, and usage tracking.
class NvmDataPartition : public NvmPartition {
 protected:
  /// Header offsets
  static const uint8_t OFF_MAGIC = 0;
  static const uint8_t OFF_VERSION = 4;
  static const uint8_t OFF_TYPE = 5;
  static const uint8_t OFF_RESERVED = 6;
  static const uint8_t OFF_SIZE = 8;
  static const uint8_t OFF_FIRST_FREE = 12;

  /// Header constants
  static const uint32_t HEADER_SIZE = 16;  ///< magic(4) + version(1) + type(1) + reserved(2) + size(4) + first_free(4)
  static const uint32_t MAGIC = 0x4B565354;  ///< "KVST" - unified magic for all NVM partitions
  static const uint8_t VERSION = 1;          ///< Version 1 - unified across all partitions

  /// Usage warning thresholds
  static constexpr float WARNING_L1_PERCENT = 80.0f;
  static constexpr float WARNING_L2_PERCENT = 90.0f;

 public:
  NvmDataPartition(NvmPlatform *parent, const PartitionConfig &config)
      : NvmPartition(parent, config), initialized_(false), warned_L1_percent_(false) {}

 protected:
  /// Validate header and check if reinitialization is needed
  /// @param expected_type The expected partition type
  /// @return true if header is valid and matches expected type
  bool validate_header_(PartitionType expected_type);

  /// Write the header to the partition
  /// @param partition_size Total size of the partition
  /// @param type Partition type
  /// @param first_free First free offset (usually HEADER_SIZE)
  void write_header_(uint32_t partition_size, PartitionType type, uint32_t first_free);

  /// Read first_free offset from header
  /// @return First free offset, or HEADER_SIZE if not set
  uint32_t read_first_free_();

  /// Update first_free offset in header
  /// @param first_free New first free offset
  void update_first_free_(uint32_t first_free);

  /// Check usage and warn if approaching capacity
  /// @param used Number of bytes used
  /// @param total Total partition size
  void check_usage_warnings_(uint32_t used, uint32_t total);

  /// Log a header mismatch
  /// @param issue Description of the issue
  /// @param actual Actual value found
  /// @param expected Expected value
  void log_mismatch_(const char *issue, uint32_t actual, uint32_t expected);

 public:
  virtual ~NvmDataPartition() = default;

  /// Get the percentage of partition space used
  /// @return Usage percentage (0-100)
  float get_usage_percent();

  bool initialized_;        ///< Track if partition has been initialized
  bool warned_L1_percent_;  ///< Track if L1 warning was issued this boot
};

/// Specialized partition for preferences storage
///
/// This partition type integrates with ESPHome's preferences system,
/// allowing global variables and other preferences to be stored in
/// external NVM instead of flash.
///
/// When created, this partition automatically registers itself as the
/// global preferences backend, replacing the default flash-based storage.
class PreferencesPartition : public NvmDataPartition, public Component, public ESPPreferences {
 public:
  using NvmDataPartition::NvmDataPartition;

  /// Setup the preferences backend
  void setup() override;

  /// Run after FRAM (IO) but before components that use preferences (BUS)
  float get_setup_priority() const override { return setup_priority::BUS; }

  /// Dump configuration
  void dump_config() override;

  // ========== ESPPreferences interface ==========
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override;
  ESPPreferenceObject make_preference(size_t length, uint32_t type) override;
  bool sync() override;
  bool reset() override;

 protected:
  friend class NvmPreferenceBackend;

  /// Initialize the preferences pool
  void ensure_initialized_();

  /// Calculate pool usage
  uint32_t calculate_pool_used_();

  ESPPreferences *nvs_preferences_{nullptr};  ///< Original NVS preferences for delegated keys
  bool pool_cleared_{false};
  uint32_t pool_used_{0};
};

/// Backend for individual preference objects
class NvmPreferenceBackend : public ESPPreferenceBackend {
 public:
  NvmPreferenceBackend(PreferencesPartition *partition, uint32_t type) : partition_(partition), type_(type) {}

  bool save(const uint8_t *data, size_t len) override;
  bool load(uint8_t *data, size_t len) override;

 protected:
  /// Find or allocate a key slot
  uint32_t find_key_(uint32_t key_hash);

  PreferencesPartition *partition_;
  uint32_t type_;
};

/// Specialized partition for raw data storage
///
/// This partition type provides direct byte-level access to the storage.
/// Users are responsible for managing the data structure and format.
class RawPartition : public NvmPartition {
 public:
  using NvmPartition::NvmPartition;

  /// Read a value from the partition at the specified offset
  /// @tparam T Type of value to read
  /// @param offset Byte offset within partition
  /// @param value Reference to store the read value
  /// @return true on success
  template<typename T> bool read_value(uint32_t offset, T &value) {
    return read(offset, reinterpret_cast<uint8_t *>(&value), sizeof(T));
  }

  /// Write a value to the partition at the specified offset
  /// @tparam T Type of value to write
  /// @param offset Byte offset within partition
  /// @param value Value to write
  /// @return true on success
  template<typename T> bool write_value(uint32_t offset, const T &value) {
    return write(offset, reinterpret_cast<const uint8_t *>(&value), sizeof(T));
  }
};

/// Specialized partition for key-value storage
///
/// This partition type provides a simple key-value store where values
/// can be stored and retrieved by string keys. The storage format is:
/// Header (16 bytes): [magic: 4][version: 1][type: 1][reserved: 2][size: 4][first_free: 4]
///   - magic: 0x4B565354 ("KVST")
///   - version: format version (1)
///   - type: PartitionType (2 = KEY_VALUE)
///   - reserved: reserved for future use
///   - size: partition size in bytes
///   - first_free: offset of first free slot (for O(1) usage tracking)
/// Entries: [key_len: 1 byte][key: N bytes][value_len: 2 bytes][value: M bytes]
class KeyValuePartition : public NvmDataPartition, public Component {
 public:
  using NvmDataPartition::NvmDataPartition;

  /// Dump configuration for debugging
  void dump_config() override;

  /// Get value by key
  /// @param key Key to look up
  /// @param value Buffer to store value
  /// @param max_len Maximum bytes to read
  /// @return Actual bytes read, or -1 on error
  int get(const std::string &key, uint8_t *value, size_t max_len);

  /// Set value by key
  /// @param key Key to set
  /// @param value Value to store
  /// @param len Length of value
  /// @return true on success
  bool set(const std::string &key, const uint8_t *value, size_t len);

  /// Delete key
  /// @param key Key to delete
  /// @return true on success
  bool erase(const std::string &key);

  /// Check if key exists
  /// @param key Key to check
  /// @return true if key exists
  bool has_key(const std::string &key);

  /// Get value as string into a buffer (no heap allocation)
  /// @param key Key to look up
  /// @param buf Buffer to store the string
  /// @param buf_len Size of buffer
  /// @param default_value Value to use if key not found
  /// @return Actual length of string (excluding null terminator), or -1 on error
  int get_string(const std::string &key, char *buf, size_t buf_len, const char *default_value = "");

  /// Get value as string (heap-allocating, use sparingly)
  /// @param key Key to look up
  /// @param default_value Value to return if key not found
  /// @return The stored value or default_value
  /// @deprecated Use get_string(key, buf, buf_len, default) instead to avoid heap allocation
  std::string get_string(const std::string &key, const std::string &default_value = "");

  /// Set value as string
  /// @param key Key to set
  /// @param value String value to store
  /// @return true on success
  bool set_string(const std::string &key, const std::string &value);

  /// Get the number of bytes used in the partition
  /// @return Bytes used (including entry overhead)
  uint32_t get_used_bytes();

  /// Get the percentage of partition space used
  /// @return Usage percentage (0-100)
  float get_usage_percent();

 protected:
  /// Initialize partition header (lazy initialization)
  void ensure_initialized_();

  /// Clear partition and write new header
  void clear_partition_();

  /// Find key entry in storage
  /// @param key Key to find
  /// @param offset Output: offset where key starts (relative to data area)
  /// @param value_offset Output: offset where value starts (relative to data area)
  /// @param value_len Output: length of value
  /// @return true if key found
  bool find_key(const std::string &key, uint32_t &offset, uint32_t &value_offset, uint16_t &value_len);

  /// Compact storage (remove deleted entries)
  void compact();

  /// Calculate used bytes by scanning all entries
  /// @return Total bytes used (including header)
  uint32_t calculate_used_bytes_();

  /// Check usage and warn if approaching capacity
  void check_usage_();
};

}  // namespace nvm
}  // namespace esphome

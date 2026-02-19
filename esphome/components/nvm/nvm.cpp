#include "nvm.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <array>
#include <cstring>

namespace esphome {
namespace nvm {

static const char *const TAG = "nvm";

// Static member definitions for NvmDataPartition
const uint8_t NvmDataPartition::OFF_MAGIC;
const uint8_t NvmDataPartition::OFF_VERSION;
const uint8_t NvmDataPartition::OFF_TYPE;
const uint8_t NvmDataPartition::OFF_RESERVED;
const uint8_t NvmDataPartition::OFF_SIZE;
const uint8_t NvmDataPartition::OFF_FIRST_FREE;
const uint32_t NvmDataPartition::HEADER_SIZE;
const uint32_t NvmDataPartition::MAGIC;
const uint8_t NvmDataPartition::VERSION;

const char *partition_type_to_string(PartitionType type) {
  switch (type) {
    case PartitionType::PREFERENCES:
      return "preferences";
    case PartitionType::RAW:
      return "raw";
    case PartitionType::KEY_VALUE:
      return "key_value";
    default:
      return "unknown";
  }
}

// ========== NvmPartition ==========

NvmPartition::NvmPartition(NvmPlatform *parent, const PartitionConfig &config) : parent_(parent), config_(config) {}

bool NvmPartition::read(uint32_t offset, uint8_t *data, size_t len) {
  if (offset + len > config_.size) {
    ESP_LOGE(TAG, "Partition '%s' read out of bounds: offset=%u, len=%zu, size=%u", config_.id.c_str(), offset, len,
             config_.size);
    return false;
  }
  ESP_LOGV(TAG, "Partition '%s' read: offset=%u, len=%zu", config_.id.c_str(), offset, len);
  return parent_->read_bytes(config_.offset + offset, data, len);
}

bool NvmPartition::write(uint32_t offset, const uint8_t *data, size_t len) {
  if (offset + len > config_.size) {
    ESP_LOGE(TAG, "Partition '%s' write out of bounds: offset=%u, len=%zu, size=%u", config_.id.c_str(), offset, len,
             config_.size);
    return false;
  }
  ESP_LOGV(TAG, "Partition '%s' write: offset=%u, len=%zu", config_.id.c_str(), offset, len);
  return parent_->write_bytes(config_.offset + offset, data, len);
}

// ========== NvmPlatform ==========

void NvmPlatform::setup() {
  // Calculate automatic offsets for partitions
  this->calculate_partition_offsets();

  // Validate all partitions
  for (const auto &partition : partitions_) {
    if (!this->validate_partition_config(partition->config_)) {
      this->mark_failed();
      return;
    }
  }

  // Note: PreferencesPartition is registered with App in add_partition(),
  // so its setup() will be called automatically by the Application

  ESP_LOGCONFIG(TAG, "NVM Platform initialized with %zu partitions", partitions_.size());
}

void NvmPlatform::dump_config() {
  ESP_LOGCONFIG(TAG, "NVM Platform:");
  ESP_LOGCONFIG(TAG, "  Total size: %u bytes", this->get_total_size());
  ESP_LOGCONFIG(TAG, "  Partitions: %zu", partitions_.size());

  for (const auto &partition : partitions_) {
    ESP_LOGCONFIG(TAG, "    '%s': type=%s, offset=0x%04X, size=%u bytes", partition->get_id().c_str(),
                  partition_type_to_string(partition->get_type()), partition->get_offset(), partition->get_size());
    // Call partition-specific dump_config for detailed info
    if (partition->get_type() == PartitionType::KEY_VALUE) {
      static_cast<KeyValuePartition *>(partition.get())->dump_config();
    }
    // PreferencesPartition has its own dump_config called by Application (it's a Component)
  }
}

NvmPartition *NvmPlatform::add_partition(const PartitionConfig &config) {
  // Create appropriate partition type
  std::unique_ptr<NvmPartition> partition;

  switch (config.type) {
    case PartitionType::PREFERENCES: {
      auto pref_partition = std::make_unique<PreferencesPartition>(this, config);
      // Register with Application so setup() is called automatically
      App.register_component(pref_partition.get());
      partition = std::move(pref_partition);
      break;
    }
    case PartitionType::RAW:
      partition = std::make_unique<RawPartition>(this, config);
      break;
    case PartitionType::KEY_VALUE:
      partition = std::make_unique<KeyValuePartition>(this, config);
      break;
    default:
      ESP_LOGE(TAG, "Unknown partition type: %d", static_cast<int>(config.type));
      return nullptr;
  }

  ESP_LOGD(TAG, "Configured partition '%s': type=%s, offset=0x%04X, size=%u bytes", config.id.c_str(),
           partition_type_to_string(config.type), config.offset, config.size);

  partitions_.push_back(std::move(partition));
  return partitions_.back().get();
}

NvmPartition *NvmPlatform::get_partition(const std::string &id) {
  for (const auto &partition : partitions_) {
    if (partition->get_id() == id) {
      return partition.get();
    }
  }
  return nullptr;
}

bool NvmPlatform::validate_partition_config(const PartitionConfig &config) {
  // Check size
  if (config.size == 0) {
    ESP_LOGE(TAG, "Partition '%s' has zero size", config.id.c_str());
    return false;
  }

  // Check bounds
  if (config.offset + config.size > this->get_total_size()) {
    ESP_LOGE(TAG, "Partition '%s' exceeds device size: offset=%u, size=%u, device_size=%u", config.id.c_str(),
             config.offset, config.size, this->get_total_size());
    return false;
  }

  // Check for overlap
  if (!this->check_partition_overlap(config)) {
    return false;
  }

  return true;
}

bool NvmPlatform::check_partition_overlap(const PartitionConfig &config) {
  for (const auto &partition : partitions_) {
    // Skip the partition being validated (for updates)
    if (partition->get_id() == config.id) {
      continue;
    }

    // Check for overlap
    uint32_t existing_end = partition->get_offset() + partition->get_size();
    uint32_t new_end = config.offset + config.size;

    if (config.offset < existing_end && new_end > partition->get_offset()) {
      ESP_LOGE(TAG, "Partition '%s' overlaps with partition '%s'", config.id.c_str(), partition->get_id().c_str());
      return false;
    }
  }
  return true;
}

void NvmPlatform::calculate_partition_offsets() {
  uint32_t current_offset = 0;

  for (auto &partition : partitions_) {
    // If offset is 0, auto-calculate
    if (partition->config_.offset == 0) {
      partition->config_.offset = current_offset;
    }

    // Update current offset for next partition
    current_offset = partition->config_.offset + partition->config_.size;
  }
}

// ========== KeyValuePartition ==========

int KeyValuePartition::get(const std::string &key, uint8_t *value, size_t max_len) {
  // Ensure partition is initialized
  this->ensure_initialized_();

  ESP_LOGV(TAG, "KeyValue '%s' get: key='%s', max_len=%zu", this->get_id().c_str(), key.c_str(), max_len);

  uint32_t offset, value_offset;
  uint16_t value_len;

  if (!this->find_key(key, offset, value_offset, value_len)) {
    ESP_LOGV(TAG, "KeyValue '%s' get: key='%s' not found", this->get_id().c_str(), key.c_str());
    return -1;  // Key not found
  }

  // Limit read length
  size_t read_len = std::min(static_cast<size_t>(value_len), max_len);
  // value_offset is relative to data area, need to add HEADER_SIZE for absolute partition offset
  if (!this->read(HEADER_SIZE + value_offset, value, read_len)) {
    return -1;
  }

  ESP_LOGV(TAG, "KeyValue '%s' get: key='%s' found, len=%zu", this->get_id().c_str(), key.c_str(), read_len);
  return static_cast<int>(read_len);
}

bool KeyValuePartition::set(const std::string &key, const uint8_t *value, size_t len) {
  // Ensure partition is initialized
  this->ensure_initialized_();

  ESP_LOGV(TAG, "KeyValue '%s' set: key='%s', len=%zu", this->get_id().c_str(), key.c_str(), len);

  // Check if key already exists
  uint32_t existing_offset, value_offset;
  uint16_t existing_len;

  if (this->find_key(key, existing_offset, value_offset, existing_len)) {
    // Key exists - check if new value fits
    if (len <= existing_len) {
      // Overwrite in place (add HEADER_SIZE to convert from data-relative to absolute)
      return this->write(HEADER_SIZE + value_offset, value, len);
    } else {
      // Need to erase and rewrite
      this->erase(key);
    }
  }

  // Find end of storage (start after header)
  uint32_t write_offset = HEADER_SIZE;
  uint8_t marker;
  bool is_first_entry = (write_offset == HEADER_SIZE);  // Track if this is the first entry
  while (write_offset < this->get_size()) {
    if (!this->read(write_offset, &marker, 1)) {
      break;
    }
    if (marker == 0xFF || marker == 0x00) {
      // Empty slot found
      break;
    }
    is_first_entry = false;  // Found an existing entry, so not first time
    // Skip entry: key_len(1) + key + value_len(2) + value
    uint8_t key_len = marker;
    uint16_t value_len;
    this->read(write_offset + 1 + key_len, reinterpret_cast<uint8_t *>(&value_len), 2);
    write_offset += 1 + key_len + 2 + value_len;
  }

  // Check if we have space
  size_t entry_size = 1 + key.size() + 2 + len;
  if (write_offset + entry_size > this->get_size()) {
    ESP_LOGE(TAG, "KeyValue partition '%s' full, cannot store key '%s'", this->get_id().c_str(), key.c_str());
    return false;
  }

  // Write entry
  uint8_t key_len = static_cast<uint8_t>(key.size());
  uint16_t value_len = static_cast<uint16_t>(len);

  this->write(write_offset, &key_len, 1);
  this->write(write_offset + 1, reinterpret_cast<const uint8_t *>(key.c_str()), key.size());
  this->write(write_offset + 1 + key.size(), reinterpret_cast<uint8_t *>(&value_len), 2);
  this->write(write_offset + 1 + key.size() + 2, value, len);

  ESP_LOGV(TAG, "KeyValue '%s' set: key='%s' written at offset=%u", this->get_id().c_str(), key.c_str(), write_offset);

  // Update first_free_offset if we wrote past it
  uint32_t new_first_free = write_offset + entry_size;
  if (new_first_free > HEADER_SIZE) {
    this->write(12, reinterpret_cast<uint8_t *>(&new_first_free), 4);
    ESP_LOGVV(TAG, "KeyValue '%s' updated first_free to %u", this->get_id().c_str(), new_first_free);
  }

  // Check usage and warn if approaching capacity
  this->check_usage_();

  return true;
}

bool KeyValuePartition::erase(const std::string &key) {
  // Ensure partition is initialized
  this->ensure_initialized_();

  ESP_LOGV(TAG, "KeyValue '%s' erase: key='%s'", this->get_id().c_str(), key.c_str());

  uint32_t offset, value_offset;
  uint16_t value_len;

  if (!this->find_key(key, offset, value_offset, value_len)) {
    ESP_LOGV(TAG, "KeyValue '%s' erase: key='%s' not found", this->get_id().c_str(), key.c_str());
    return false;  // Key not found
  }

  // Mark as deleted by zeroing key length (need to add HEADER_SIZE to convert to absolute offset)
  uint8_t zero = 0;
  this->write(HEADER_SIZE + offset, &zero, 1);

  ESP_LOGV(TAG, "KeyValue '%s' erase: key='%s' erased", this->get_id().c_str(), key.c_str());
  return true;
}

bool KeyValuePartition::has_key(const std::string &key) {
  uint32_t offset, value_offset;
  uint16_t value_len;
  return this->find_key(key, offset, value_offset, value_len);
}

int KeyValuePartition::get_string(const std::string &key, char *buf, size_t buf_len, const char *default_value) {
  if (buf == nullptr || buf_len == 0) {
    return -1;
  }

  int len = this->get(key, reinterpret_cast<uint8_t *>(buf), buf_len - 1);
  if (len < 0) {
    // Key not found, copy default value
    if (default_value != nullptr) {
      size_t default_len = strlen(default_value);
      size_t copy_len = std::min(default_len, buf_len - 1);
      memcpy(buf, default_value, copy_len);
      buf[copy_len] = '\0';
      return static_cast<int>(copy_len);
    }
    buf[0] = '\0';
    return 0;
  }

  // Null-terminate the string
  buf[len] = '\0';
  return len;
}

std::string KeyValuePartition::get_string(const std::string &key, const std::string &default_value) {
  // Use std::array for compile-time known size (avoid STL vector overhead)
  std::array<uint8_t, 256> buffer{};
  int len = this->get(key, buffer.data(), buffer.size());
  if (len < 0) {
    return default_value;
  }
  return std::string(buffer.begin(), buffer.begin() + len);
}

bool KeyValuePartition::set_string(const std::string &key, const std::string &value) {
  return this->set(key, reinterpret_cast<const uint8_t *>(value.c_str()), value.size());
}

bool KeyValuePartition::find_key(const std::string &key, uint32_t &offset, uint32_t &value_offset,
                                 uint16_t &value_len) {
  // Start after header
  uint32_t current_offset = HEADER_SIZE;

  while (current_offset < this->get_size()) {
    uint8_t key_len;
    if (!this->read(current_offset, &key_len, 1)) {
      break;
    }

    // Check for empty slot
    if (key_len == 0xFF || key_len == 0x00) {
      break;
    }

    // Check if key matches
    if (key_len == key.size()) {
      // Use unique_ptr for runtime-sized buffer (avoid STL vector overhead)
      auto stored_key = std::make_unique<uint8_t[]>(key_len);
      this->read(current_offset + 1, stored_key.get(), key_len);

      if (std::string(stored_key.get(), stored_key.get() + key_len) == key) {
        // Found it! Return offsets relative to data area (without header)
        offset = current_offset - HEADER_SIZE;
        this->read(current_offset + 1 + key_len, reinterpret_cast<uint8_t *>(&value_len), 2);
        value_offset = current_offset + 1 + key_len + 2 - HEADER_SIZE;
        return true;
      }
    }

    // Skip to next entry
    uint16_t entry_value_len;
    this->read(current_offset + 1 + key_len, reinterpret_cast<uint8_t *>(&entry_value_len), 2);
    current_offset += 1 + key_len + 2 + entry_value_len;
  }

  return false;
}

void KeyValuePartition::compact() {
  // TODO: Implement compaction to remove deleted entries
  // This would read all valid entries, erase the partition, and rewrite them
}

uint32_t KeyValuePartition::calculate_used_bytes_() {
  // Start after header and include header in used bytes
  uint32_t current_offset = HEADER_SIZE;
  uint32_t used_bytes = HEADER_SIZE;  // Account for header

  while (current_offset < this->get_size()) {
    uint8_t key_len;
    if (!this->read(current_offset, &key_len, 1)) {
      break;
    }

    // Check for empty slot
    if (key_len == 0xFF || key_len == 0x00) {
      break;
    }

    // Skip deleted entries (key_len == 0 from erase())
    if (key_len == 0) {
      // Read the old value length to skip this entry
      // Note: after key_len(0), there's still the old key + value_len + value
      // But we can't know the key length anymore, so we need to scan differently
      // For now, just count it as used space (compaction will recover it)
      // Read value_len at offset + 1 (where key would have been, but we don't know its length)
      // This is a limitation - deleted entries are counted as used until compaction
      // For simplicity, we'll stop counting here since we can't reliably skip
      break;
    }

    // Read value length
    uint16_t value_len;
    this->read(current_offset + 1 + key_len, reinterpret_cast<uint8_t *>(&value_len), 2);

    // Add entry size: key_len(1) + key + value_len(2) + value
    used_bytes += 1 + key_len + 2 + value_len;
    current_offset += 1 + key_len + 2 + value_len;
  }

  return used_bytes;
}

uint32_t KeyValuePartition::get_used_bytes() {
  // Use first_free_offset from header for O(1) calculation
  this->ensure_initialized_();

  uint32_t first_free = HEADER_SIZE;
  this->read(12, reinterpret_cast<uint8_t *>(&first_free), 4);

  // If first_free is 0 or invalid, fall back to scanning
  if (first_free < HEADER_SIZE || first_free > this->get_size()) {
    return this->calculate_used_bytes_();
  }

  ESP_LOGVV(TAG, "KeyValue '%s' get_used_bytes: first_free=%u", this->get_id().c_str(), first_free);
  return first_free;
}

float KeyValuePartition::get_usage_percent() {
  uint32_t used = this->get_used_bytes();
  return (used * 100.0f) / this->get_size();
}

void KeyValuePartition::check_usage_() {
  float usage_percent = this->get_usage_percent();

  if (usage_percent > WARNING_L2_PERCENT) {
    ESP_LOGW(TAG, "KeyValue partition '%s' is %.0f%% full! Consider increasing partition size", this->get_id().c_str(),
             usage_percent);
  } else if (usage_percent > WARNING_L1_PERCENT && !this->warned_L1_percent_) {
    ESP_LOGW(TAG, "KeyValue partition '%s' is %.0f%% full. Consider increasing partition size soon",
             this->get_id().c_str(), usage_percent);
    this->warned_L1_percent_ = true;
  }
}

void KeyValuePartition::dump_config() {
  ESP_LOGCONFIG(TAG, "KeyValue Partition '%s':", this->get_id().c_str());
  ESP_LOGCONFIG(TAG, "  Size: %u bytes", this->get_size());
  uint32_t used = this->get_used_bytes();
  if (used > 0) {
    float usage_percent = this->get_usage_percent();
    ESP_LOGCONFIG(TAG, "  Used: %u bytes (%.1f%%)", used, usage_percent);
  }
  // Initialize if needed to show header info
  this->ensure_initialized_();
  if (this->initialized_) {
    uint32_t magic = 0;
    uint8_t version = 0;
    uint8_t type = 0;
    uint32_t stored_size = 0;
    uint32_t first_free = 0;
    this->read(0, reinterpret_cast<uint8_t *>(&magic), 4);
    this->read(4, &version, 1);
    this->read(5, &type, 1);
    this->read(8, reinterpret_cast<uint8_t *>(&stored_size), 4);
    this->read(12, reinterpret_cast<uint8_t *>(&first_free), 4);
    ESP_LOGCONFIG(TAG, "  Header: magic=0x%08X, version=%u, type=%u, size=%u, first_free=%u", magic, version, type,
                  stored_size, first_free);
  }
}

void KeyValuePartition::ensure_initialized_() {
  if (this->initialized_) {
    return;
  }

  uint32_t magic = 0;
  this->read(0, reinterpret_cast<uint8_t *>(&magic), 4);
  ESP_LOGVV(TAG, "KeyValue '%s' init: magic=0x%08X, expected=0x%08X", this->get_id().c_str(), magic, MAGIC);

  bool needs_clear = false;

  if (magic != MAGIC) {
    ESP_LOGW(TAG, "KeyValue partition '%s' has invalid magic (0x%08X), initializing", this->get_id().c_str(), magic);
    needs_clear = true;
  } else {
    uint8_t version = 0;
    this->read(4, &version, 1);
    ESP_LOGVV(TAG, "KeyValue '%s' init: version=%u, expected=%u", this->get_id().c_str(), version, VERSION);
    if (version != VERSION) {
      ESP_LOGW(TAG, "KeyValue partition '%s' version mismatch (%u vs %u), reinitializing", this->get_id().c_str(),
               version, VERSION);
      needs_clear = true;
    } else {
      // Validate partition type
      uint8_t type = 0;
      this->read(5, &type, 1);
      ESP_LOGVV(TAG, "KeyValue '%s' init: type=%u, expected=%u", this->get_id().c_str(), type,
                static_cast<uint8_t>(PartitionType::KEY_VALUE));
      if (type != static_cast<uint8_t>(PartitionType::KEY_VALUE)) {
        ESP_LOGW(TAG, "KeyValue partition '%s' type mismatch (%u vs %u), reinitializing", this->get_id().c_str(), type,
                 static_cast<uint8_t>(PartitionType::KEY_VALUE));
        needs_clear = true;
      } else {
        // Validate stored partition size
        uint32_t stored_size = 0;
        this->read(8, reinterpret_cast<uint8_t *>(&stored_size), 4);
        if (stored_size != this->get_size()) {
          ESP_LOGW(TAG, "KeyValue partition '%s' size changed (%u vs %u), reinitializing", this->get_id().c_str(),
                   stored_size, this->get_size());
          needs_clear = true;
        } else {
          // Validate first_free_offset - scan to find actual end if needed
          uint32_t first_free = 0;
          this->read(12, reinterpret_cast<uint8_t *>(&first_free), 4);
          ESP_LOGVV(TAG, "KeyValue '%s' init: first_free=%u", this->get_id().c_str(), first_free);

          // Sanity check: if first_free is beyond partition size, reinitialize
          if (first_free > this->get_size()) {
            ESP_LOGW(TAG, "KeyValue partition '%s' has invalid first_free offset (%u), reinitializing",
                     this->get_id().c_str(), first_free);
            needs_clear = true;
          }
        }
      }
    }
  }

  if (needs_clear) {
    this->clear_partition_();
  }

  this->initialized_ = true;
}

void KeyValuePartition::clear_partition_() {
  ESP_LOGI(TAG, "Created partition '%s': type=key_value, size=%u bytes", this->get_id().c_str(), this->get_size());

  uint32_t partition_size = this->get_size();

  // Clear the entire partition
  for (uint32_t i = 0; i < partition_size; i += 32) {
    std::array<uint8_t, 32> zeros{};
    uint32_t len = (i + 32 > partition_size) ? (partition_size - i) : 32;
    this->write(i, zeros.data(), len);
  }

  // Write header
  uint32_t magic = MAGIC;
  this->write(0, reinterpret_cast<uint8_t *>(&magic), 4);

  uint8_t version = VERSION;
  this->write(4, &version, 1);

  uint8_t type = static_cast<uint8_t>(PartitionType::KEY_VALUE);
  this->write(5, &type, 1);

  uint16_t reserved = 0;
  this->write(6, reinterpret_cast<uint8_t *>(&reserved), 2);

  uint32_t size = partition_size;
  this->write(8, reinterpret_cast<uint8_t *>(&size), 4);

  // Write first free offset (initially after header)
  uint32_t first_free = HEADER_SIZE;
  this->write(12, reinterpret_cast<uint8_t *>(&first_free), 4);

  // Verify write
  uint32_t verify_magic = 0;
  this->read(0, reinterpret_cast<uint8_t *>(&verify_magic), 4);
  ESP_LOGVV(TAG, "KeyValue '%s' verify: magic=0x%08X", this->get_id().c_str(), verify_magic);
}

// ========== NvmDataPartition ==========

bool NvmDataPartition::validate_header_(PartitionType expected_type) {
  uint32_t magic = 0;
  this->read(OFF_MAGIC, reinterpret_cast<uint8_t *>(&magic), 4);

  if (magic != MAGIC) {
    return false;
  }

  uint8_t version = 0;
  this->read(OFF_VERSION, &version, 1);
  if (version != VERSION) {
    return false;
  }

  uint8_t type = 0;
  this->read(OFF_TYPE, &type, 1);
  if (type != static_cast<uint8_t>(expected_type)) {
    return false;
  }

  uint32_t stored_size = 0;
  this->read(OFF_SIZE, reinterpret_cast<uint8_t *>(&stored_size), 4);
  if (stored_size != this->get_size()) {
    return false;
  }

  return true;
}

void NvmDataPartition::write_header_(uint32_t partition_size, PartitionType type, uint32_t first_free) {
  uint32_t magic = MAGIC;
  uint8_t version = VERSION;
  uint8_t type_val = static_cast<uint8_t>(type);
  uint16_t reserved = 0;

  this->write(OFF_MAGIC, reinterpret_cast<uint8_t *>(&magic), 4);
  this->write(OFF_VERSION, &version, 1);
  this->write(OFF_TYPE, &type_val, 1);
  this->write(OFF_RESERVED, reinterpret_cast<uint8_t *>(&reserved), 2);
  this->write(OFF_SIZE, reinterpret_cast<uint8_t *>(&partition_size), 4);
  this->write(OFF_FIRST_FREE, reinterpret_cast<uint8_t *>(&first_free), 4);
}

uint32_t NvmDataPartition::read_first_free_() {
  uint32_t first_free = HEADER_SIZE;
  this->read(OFF_FIRST_FREE, reinterpret_cast<uint8_t *>(&first_free), 4);
  return first_free;
}

void NvmDataPartition::update_first_free_(uint32_t first_free) {
  this->write(OFF_FIRST_FREE, reinterpret_cast<uint8_t *>(&first_free), 4);
}

void NvmDataPartition::check_usage_warnings_(uint32_t used, uint32_t total) {
  float usage_percent = (used * 100.0f) / total;

  if (usage_percent > WARNING_L2_PERCENT) {
    ESP_LOGW(TAG, "Partition '%s' is %.0f%% full! Consider increasing partition size", this->get_id().c_str(),
             usage_percent);
  } else if (usage_percent > WARNING_L1_PERCENT && !this->warned_L1_percent_) {
    ESP_LOGW(TAG, "Partition '%s' is %.0f%% full. Consider increasing partition size soon", this->get_id().c_str(),
             usage_percent);
    this->warned_L1_percent_ = true;
  }
}

void NvmDataPartition::log_mismatch_(const char *issue, uint32_t actual, uint32_t expected) {
  ESP_LOGW(TAG, "Partition '%s' %s (actual=0x%08X, expected=0x%08X)", this->get_id().c_str(), issue, actual, expected);
}

float NvmDataPartition::get_usage_percent() {
  uint32_t first_free = this->read_first_free_();
  float usage = (first_free * 100.0f) / this->get_size();
  return usage;
}

// ========== PreferencesPartition ==========

void PreferencesPartition::setup() {
  ESP_LOGVV(TAG, "PreferencesPartition setup: size=%u", this->get_size());

  // Initialize the pool
  this->ensure_initialized_();

  // Save the original NVS preferences before replacing
  // This is needed to delegate the boot counter key which must stay in NVS
  // because safe_mode reads it before NVM preferences is initialized
  this->nvs_preferences_ = global_preferences;

  // Set global preferences pointer
  global_preferences = this;

  ESP_LOGVV(TAG, "PreferencesPartition setup complete, initialized=%d", this->initialized_);
}

void PreferencesPartition::dump_config() {
  ESP_LOGCONFIG(TAG, "Preferences Partition:");
  ESP_LOGCONFIG(TAG, "  Size: %u bytes", this->get_size());
  if (this->pool_used_ > 0) {
    float usage_percent = (this->pool_used_ * 100.0f) / this->get_size();
    ESP_LOGCONFIG(TAG, "  Used: %u bytes (%.1f%%)", this->pool_used_, usage_percent);
  }
  if (this->pool_cleared_) {
    ESP_LOGCONFIG(TAG, "  Pool was cleared");
  }
  ESP_LOGCONFIG(TAG, "  Boot counter: delegated to NVS");
}

ESPPreferenceObject PreferencesPartition::make_preference(size_t length, uint32_t type, bool in_flash) {
  return this->make_preference(length, type);
}

ESPPreferenceObject PreferencesPartition::make_preference(size_t length, uint32_t type) {
  // Delegate boot counter to NVS - safe_mode reads it before NVM is initialized
  if (type == safe_mode::RTC_KEY && this->nvs_preferences_ != nullptr) {
    ESP_LOGV(TAG, "Delegating boot counter key %u to NVS", type);
    return this->nvs_preferences_->make_preference(length, type);
  }
  return ESPPreferenceObject(new NvmPreferenceBackend(this, type));
}

bool PreferencesPartition::sync() {
  // Also sync NVS preferences (for delegated keys like boot counter)
  if (this->nvs_preferences_ != nullptr) {
    this->nvs_preferences_->sync();
  }
  return true;
}

bool PreferencesPartition::reset() {
  this->pool_cleared_ = true;
  uint32_t pool_size = this->get_size();

  // Clear the entire pool area
  for (uint32_t i = 0; i < pool_size; i += 32) {
    std::array<uint8_t, 32> zeros{};
    uint32_t len = (i + 32 > pool_size) ? (pool_size - i) : 32;
    this->write(i, zeros.data(), len);
  }

  // Write unified header (16 bytes): magic(4) + version(1) + type(1) + reserved(2) + size(4) + first_free(4)
  uint32_t magic = MAGIC;
  uint8_t version = VERSION;
  uint8_t type = static_cast<uint8_t>(PartitionType::PREFERENCES);
  uint16_t reserved = 0;
  uint32_t first_free = HEADER_SIZE;

  this->write(0, reinterpret_cast<uint8_t *>(&magic), 4);
  this->write(4, &version, 1);
  this->write(5, &type, 1);
  this->write(6, reinterpret_cast<uint8_t *>(&reserved), 2);
  this->write(8, reinterpret_cast<uint8_t *>(&pool_size), 4);
  this->write(12, reinterpret_cast<uint8_t *>(&first_free), 4);

  this->pool_used_ = HEADER_SIZE;
  this->warned_L1_percent_ = false;
  ESP_LOGD(TAG, "Factory reset: NVM preferences cleared");
  return true;
}

void PreferencesPartition::ensure_initialized_() {
  if (this->initialized_) {
    return;
  }

  uint32_t pool_size = this->get_size();
  ESP_LOGVV(TAG, "Lazy initialization - pool_size=%u", pool_size);

  // Read unified header (16 bytes): magic(4) + version(1) + type(1) + reserved(2) + size(4) + first_free(4)
  uint32_t magic = 0;
  this->read(0, reinterpret_cast<uint8_t *>(&magic), 4);
  ESP_LOGVV(TAG, "Read magic: 0x%08X, expected: 0x%08X", magic, MAGIC);

  bool needs_clear = false;

  if (magic != MAGIC) {
    ESP_LOGW(TAG, "Preferences partition '%s' has invalid magic (0x%08X), initializing", this->get_id().c_str(), magic);
    needs_clear = true;
  } else {
    // Magic matches - verify version and type
    uint8_t version = 0;
    this->read(4, &version, 1);
    ESP_LOGVV(TAG, "Read version: %u, expected: %u", version, VERSION);

    uint8_t type = 0;
    this->read(5, &type, 1);
    ESP_LOGVV(TAG, "Read type: %u, expected: %u", type, static_cast<uint8_t>(PartitionType::PREFERENCES));

    if (version != VERSION) {
      ESP_LOGW(TAG, "Version mismatch (%u != %u), reinitializing pool", version, VERSION);
      needs_clear = true;
    } else if (type != static_cast<uint8_t>(PartitionType::PREFERENCES)) {
      ESP_LOGW(TAG, "Type mismatch (%u != %u), reinitializing pool", type,
               static_cast<uint8_t>(PartitionType::PREFERENCES));
      needs_clear = true;
    } else {
      // Read stored pool size
      uint32_t stored_size = 0;
      this->read(8, reinterpret_cast<uint8_t *>(&stored_size), 4);
      ESP_LOGVV(TAG, "Read stored_size: %u, actual: %u", stored_size, pool_size);

      if (stored_size != pool_size) {
        ESP_LOGW(TAG, "Pool size mismatch (%u != %u), reinitializing", stored_size, pool_size);
        needs_clear = true;
      } else {
        // Read first_free offset
        uint32_t first_free = 0;
        this->read(12, reinterpret_cast<uint8_t *>(&first_free), 4);
        ESP_LOGVV(TAG, "Read first_free: %u", first_free);

        if (first_free > pool_size || first_free < HEADER_SIZE) {
          ESP_LOGW(TAG, "Invalid first_free offset (%u), reinitializing", first_free);
          needs_clear = true;
        } else {
          this->pool_used_ = first_free;
        }
      }
    }
  }

  if (!needs_clear) {
    // Calculate pool usage before checking pool size change
    this->pool_used_ = this->calculate_pool_used_();

    // Check if pool size decreased and data doesn't fit
    uint32_t stored_pool_size = 0;
    this->read(8, reinterpret_cast<uint8_t *>(&stored_pool_size), 4);
    ESP_LOGVV(TAG, "Stored pool size: %u, current: %u, used: %u", stored_pool_size, pool_size, this->pool_used_);

    if (stored_pool_size != 0 && pool_size < stored_pool_size) {
      // Pool size decreased - check if data still fits
      if (this->pool_used_ > pool_size) {
        ESP_LOGW(TAG, "Pool size decreased from %u to %u and data (%u bytes) doesn't fit! Clearing pool.",
                 stored_pool_size, pool_size, this->pool_used_);
        needs_clear = true;
      } else {
        ESP_LOGW(TAG, "Pool size decreased from %u to %u. Data (%u bytes) fits, preserving.", stored_pool_size,
                 pool_size, this->pool_used_);
      }
    }
  }

  if (!needs_clear) {
    ESP_LOGD(TAG, "NVM preferences pool restored successfully");
    float usage_percent = (this->pool_used_ * 100.0f) / pool_size;
    ESP_LOGD(TAG, "Pool usage: %u/%u bytes (%.1f%%)", this->pool_used_, pool_size, usage_percent);

    // Warn if pool is too small (over WARNING_L2_PERCENT at startup)
    if (usage_percent > WARNING_L2_PERCENT) {
      ESP_LOGW(TAG, "Pool is %.0f%% full! Consider increasing partition size", usage_percent);
    } else if (usage_percent > WARNING_L1_PERCENT) {
      ESP_LOGW(TAG, "Pool is %.0f%% full. Consider increasing partition size soon", usage_percent);
      this->warned_L1_percent_ = true;
    }
  }

  if (needs_clear) {
    this->pool_cleared_ = true;
    ESP_LOGI(TAG, "Created partition '%s': type=preferences, size=%u bytes", this->get_id().c_str(), pool_size);
    // Clear the pool area by writing zeros
    for (uint32_t i = 0; i < pool_size; i += 32) {
      std::array<uint8_t, 32> zeros{};
      uint32_t len = (i + 32 > pool_size) ? (pool_size - i) : 32;
      this->write(i, zeros.data(), len);
    }

    // Write unified header (16 bytes): magic(4) + version(1) + type(1) + reserved(2) + size(4) + first_free(4)
    uint32_t magic = MAGIC;
    uint8_t version = VERSION;
    uint8_t type = static_cast<uint8_t>(PartitionType::PREFERENCES);
    uint16_t reserved = 0;
    uint32_t first_free = HEADER_SIZE;

    this->write(0, reinterpret_cast<uint8_t *>(&magic), 4);
    this->write(4, &version, 1);
    this->write(5, &type, 1);
    this->write(6, reinterpret_cast<uint8_t *>(&reserved), 2);
    this->write(8, reinterpret_cast<uint8_t *>(&pool_size), 4);
    this->write(12, reinterpret_cast<uint8_t *>(&first_free), 4);

    // Verify write
    uint32_t verify_magic = 0;
    this->read(0, reinterpret_cast<uint8_t *>(&verify_magic), 4);
    ESP_LOGVV(TAG, "Verify magic after write: 0x%08X", verify_magic);

    // Verify first key slot is zero
    uint32_t verify_key = 0;
    this->read(HEADER_SIZE, reinterpret_cast<uint8_t *>(&verify_key), 4);
    ESP_LOGVV(TAG, "Verify first key slot: 0x%08X", verify_key);

    this->pool_used_ = HEADER_SIZE;
  }

  this->initialized_ = true;
}

uint32_t PreferencesPartition::calculate_pool_used_() {
  uint32_t pool_size = this->get_size();
  uint32_t addr = HEADER_SIZE;
  uint32_t iterations = 0;
  const uint32_t max_iterations = 100;

  while (addr < pool_size && iterations < max_iterations) {
    iterations++;
    uint32_t key = 0;
    this->read(addr, reinterpret_cast<uint8_t *>(&key), 4);

    if (key == 0) {
      // Empty slot - this is where the pool ends
      return addr;
    }

    uint32_t size = 0;
    this->read(addr + 4, reinterpret_cast<uint8_t *>(&size), 4);

    // Invalid size - return current position
    if (size > 1024 || size == 0xFFFFFFFF) {
      return addr;
    }

    addr += 8 + size + 4;  // key + size + data + hash
  }

  return addr;
}

// ========== NvmPreferenceBackend ==========

uint32_t NvmPreferenceBackend::find_key_(uint32_t key_hash) {
  // Ensure pool is initialized before searching
  this->partition_->ensure_initialized_();

  if (!this->partition_->initialized_) {
    ESP_LOGW(TAG, "find_key_: Pool not initialized");
    return 0;
  }

  uint32_t pool_size = this->partition_->get_size();
  uint32_t addr = NvmDataPartition::HEADER_SIZE;
  uint32_t iterations = 0;
  const uint32_t max_iterations = 100;  // Safety limit

  ESP_LOGV(TAG, "find_key_: looking for 0x%08X, pool_size=%u", key_hash, pool_size);

  while (addr < pool_size && iterations < max_iterations) {
    iterations++;
    uint32_t key_from_nvm = 0;
    this->partition_->read(addr, reinterpret_cast<uint8_t *>(&key_from_nvm), 4);

    if (key_from_nvm == key_hash) {
      ESP_LOGV(TAG, "Found key 0x%08X at offset %u (iteration %u)", key_hash, addr, iterations);
      return addr;
    }

    if (key_from_nvm == 0) {
      ESP_LOGV(TAG, "Empty slot at offset %u, storing key 0x%08X (iteration %u)", addr, key_hash, iterations);
      this->partition_->write(addr, reinterpret_cast<uint8_t *>(&key_hash), 4);
      return addr;
    }

    uint32_t size_from_nvm = 0;
    this->partition_->read(addr + 4, reinterpret_cast<uint8_t *>(&size_from_nvm), 4);

    // Safety check: if size is unreasonably large, clear remaining pool and use this slot
    if (size_from_nvm > 1024 || size_from_nvm == 0xFFFFFFFF) {
      ESP_LOGW(TAG, "Invalid size %u at offset %u, clearing remaining pool", size_from_nvm, addr);
      // Clear from this address to end of pool
      for (uint32_t i = addr; i < pool_size; i += 32) {
        std::array<uint8_t, 32> zeros{};
        uint32_t len = (i + 32 > pool_size) ? (pool_size - i) : 32;
        this->partition_->write(i, zeros.data(), len);
      }
      // Now use this slot
      this->partition_->write(addr, reinterpret_cast<uint8_t *>(&key_hash), 4);
      return addr;
    }

    addr += 8 + size_from_nvm + 4;  // key + size + data + crc
  }

  if (iterations >= max_iterations) {
    ESP_LOGW(TAG, "Max iterations reached in find_key_");
  }

  return 0;
}

bool NvmPreferenceBackend::save(const uint8_t *data, size_t len) {
  uint32_t key_hash = fnv1_hash(std::to_string(this->type_));
  ESP_LOGV(TAG, "Save: type=%u, key_hash=0x%08X, len=%u", this->type_, key_hash, len);

  uint32_t addr = this->find_key_(key_hash);

  if (addr == 0) {
    ESP_LOGW(TAG, "Save: Could not find/allocate slot for key 0x%08X (pool may be full)", key_hash);
    return false;
  }

  // Check if this is an update (key already exists at this address)
  uint32_t existing_key = 0;
  this->partition_->read(addr, reinterpret_cast<uint8_t *>(&existing_key), 4);
  bool is_update = (existing_key == key_hash);
  ESP_LOGV(TAG, "Save: addr=%u, existing_key=0x%08X, is_update=%d", addr, existing_key, is_update);

  // Use FNV-1a hash for data integrity
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= FNV1_PRIME;
  }

  // Write size + data + hash
  // Use unique_ptr for runtime-sized buffer (avoid STL vector overhead)
  auto buffer = std::make_unique<uint8_t[]>(4 + len + 4);
  // Size (4 bytes)
  buffer[0] = len & 0xFF;
  buffer[1] = (len >> 8) & 0xFF;
  buffer[2] = (len >> 16) & 0xFF;
  buffer[3] = (len >> 24) & 0xFF;
  // Data
  std::memcpy(buffer.get() + 4, data, len);
  // Hash (4 bytes)
  buffer[4 + len] = hash & 0xFF;
  buffer[4 + len + 1] = (hash >> 8) & 0xFF;
  buffer[4 + len + 2] = (hash >> 16) & 0xFF;
  buffer[4 + len + 3] = (hash >> 24) & 0xFF;

  ESP_LOGVV(TAG, "Save: Writing to offset %u, hash=0x%08X", addr, hash);
  this->partition_->write(addr + 4, buffer.get(), 4 + len + 4);

  // Update pool usage tracking
  uint32_t entry_size = 4 + 4 + len + 4;  // key + size + data + hash
  uint32_t new_used = addr + entry_size;
  if (new_used > this->partition_->pool_used_) {
    this->partition_->pool_used_ = new_used;
    // Update first_free_offset in header
    this->partition_->write(12, reinterpret_cast<uint8_t *>(&new_used), 4);
  }

  // Check for WARNING_L1_PERCENT warning
  float usage_percent = (this->partition_->pool_used_ * 100.0f) / this->partition_->get_size();
  if (usage_percent > NvmDataPartition::WARNING_L1_PERCENT && !this->partition_->warned_L1_percent_) {
    ESP_LOGW(TAG, "Pool is %.0f%% full (%u/%u bytes). Consider increasing partition size", usage_percent,
             this->partition_->pool_used_, this->partition_->get_size());
    this->partition_->warned_L1_percent_ = true;
  }

  return true;
}

bool NvmPreferenceBackend::load(uint8_t *data, size_t len) {
  uint32_t key_hash = fnv1_hash(std::to_string(this->type_));
  ESP_LOGV(TAG, "Load: type=%u, key_hash=0x%08X, len=%u", this->type_, key_hash, len);

  uint32_t addr = this->find_key_(key_hash);

  if (addr == 0) {
    ESP_LOGV(TAG, "Load: Key 0x%08X not found", key_hash);
    return false;
  }

  uint32_t size_from_nvm = 0;
  this->partition_->read(addr + 4, reinterpret_cast<uint8_t *>(&size_from_nvm), 4);
  ESP_LOGVV(TAG, "Load: addr=%u, size_from_nvm=%u, expected=%u", addr, size_from_nvm, len);

  if (size_from_nvm != len) {
    ESP_LOGVV(TAG, "Load: Size mismatch (got %u, expected %u) - key may be from old config", size_from_nvm, len);
    return false;
  }

  this->partition_->read(addr + 8, data, len);
  uint32_t hash_from_nvm = 0;
  this->partition_->read(addr + 8 + len, reinterpret_cast<uint8_t *>(&hash_from_nvm), 4);

  // Use FNV-1a hash for data integrity
  uint32_t hash = FNV1_OFFSET_BASIS;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= FNV1_PRIME;
  }

  ESP_LOGVV(TAG, "Load: computed hash=0x%08X, stored hash=0x%08X, match=%d", hash, hash_from_nvm,
            hash == hash_from_nvm);
  return hash == hash_from_nvm;
}

}  // namespace nvm
}  // namespace esphome

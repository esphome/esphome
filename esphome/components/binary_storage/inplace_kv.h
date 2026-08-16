#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BINARY_STORAGE_INPLACE_KV

#include "esphome/core/component.h"
#include "esphome/components/storage/storage.h"

namespace esphome {
namespace binary_storage {

// A KeyValueStorage for byte-addressable, erase-free non-volatile memory (FRAM, MRAM). It writes
// values in place -- no erase, no wear leveling, no flash-style garbage collection -- and is chosen
// purely by capability: the backing device must NOT advertise RAW_WRITE_NEEDS_ERASE.
//
// The window is split into two equal halves. One half is active and holds a log of variable-length
// entries [committed|live|key|len|value]; the commit marker is written last, so a torn write leaves
// an entry ignored rather than corrupt, and readers take the last committed+live entry for a key
// (last-wins). When the active half fills, the live entries are rebuilt into the other half and that
// half is activated by writing its generation counter last. At the switch BOTH halves hold a
// complete, correct view of the live data, so an interrupted compaction is always recoverable
// regardless of write atomicity: whichever half is selected on the next boot is consistent.
class InplaceKVStore : public storage::KeyValueStorage {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_device(storage::RawStorage *device) { this->device_ = device; }
  void set_window(uint64_t offset, uint64_t size) {
    this->offset_ = offset;
    this->size_ = size;
  }
  void set_storage_id(const char *id) { this->storage_id_ = id; }
  void set_storage_name(const char *name) { this->storage_name_ = name; }

  storage::StorageError get_info(storage::StorageInfo *info) override;

  storage::StorageError get(uint32_t key, uint8_t *buf, size_t len, size_t *got) override;
  storage::StorageError set(uint32_t key, const uint8_t *data, size_t len) override;
  storage::StorageError erase(uint32_t key) override;
  bool has(uint32_t key) override;
  storage::StorageError get_size(uint32_t key, size_t *out) override;
  storage::StorageError list_keys(bool (*callback)(uint32_t key, size_t size, void *ctx), void *ctx) override;
  storage::StorageError ensure_initialized() override;
  storage::StorageError format() override;

 protected:
  // Byte access into the window (offsets are relative to the window start).
  bool read_(uint64_t rel_offset, uint8_t *buf, size_t len);
  bool write_(uint64_t rel_offset, const uint8_t *buf, size_t len);
  bool zero_(uint64_t rel_offset, uint64_t len);

  uint64_t half_size_() const { return this->size_ / 2; }
  uint64_t inactive_half_() const { return this->active_off_ == 0 ? this->half_size_() : 0; }
  uint32_t half_generation_(uint64_t half_off);  // 0 when the half header is absent/invalid
  bool write_half_header_(uint64_t half_off, uint32_t generation);

  // Within the half at `half_off`, find the last committed+live entry for key. Fills entries_end
  // (first free offset within the half) whenever provided.
  bool find_in_(uint64_t half_off, uint32_t key, uint64_t *entry_offset, uint16_t *value_len, uint64_t *entries_end);
  void clear_live_before_(uint64_t half_off, uint32_t key, uint64_t keep_offset);
  storage::StorageError append_(uint32_t key, const uint8_t *data, size_t len);
  storage::StorageError compact_();  // rebuild live entries into the inactive half, then activate it

  storage::RawStorage *device_{nullptr};
  uint64_t offset_{0};
  uint64_t size_{0};
  const char *storage_id_{nullptr};
  const char *storage_name_{nullptr};
  uint64_t active_off_{0};
  uint32_t active_gen_{0};
  bool initialized_{false};
};

}  // namespace binary_storage
}  // namespace esphome

#endif  // USE_BINARY_STORAGE_INPLACE_KV

#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "esphome/core/defines.h"  // before CANopenNode: clang-tidy gets the CO_CONFIG_* values from it

#define OD_DEFINITION
// Use the library search path: quoted includes match our canopen.h on Windows.
#include <CANopen.h>

namespace esphome::canopen {

// Attributes of OD entries (CANopenNode's ODA_* values)
constexpr uint8_t OD_READ = ODA_SDO_R;
constexpr uint8_t OD_WRITE = ODA_SDO_W;
constexpr uint8_t OD_READ_WRITE = ODA_SDO_RW;

// Object dictionary built at runtime (CO_MULTIPLE_OD). Entries are collected with add_*() and turned into the sorted
// OD_entry_t list by finalize(). The value storage is owned by the caller and must outlive the OD. All allocation
// happens before finalize() (in setup): the containers only grow until then, and entries refer to their objects by
// position, so the pointers CANopenNode gets are taken once they no longer move.
class ObjectDictionary {
 public:
  struct RecordSub {
    uint8_t sub;
    OD_attr_t attr;
    void *data;
    OD_size_t len;
  };

  void add_var(uint16_t index, OD_attr_t attr, void *data, OD_size_t len);
  // ARRAY: sub0 = *count (read only), subs 1..n = elements of elem_len bytes at data
  void add_array(uint16_t index, OD_attr_t attr, uint8_t *count, void *data, OD_size_t elem_len, uint8_t n);
  // RECORD: subs are looked up by subindex, so sub0 (highest subindex, uint8_t) is passed like the others
  void add_record(uint16_t index, std::initializer_list<RecordSub> subs) {
    this->add_record(index, subs.begin(), subs.size());
  }
  void add_record(uint16_t index, const RecordSub *subs, size_t count);
  // read-only VISIBLE_STRING VAR. Without ODA_STR the SDO server indicates the exact size (short strings go
  // expedited). An empty string is one zero byte: CANopenNode aborts zero-length uploads.
  void add_string(uint16_t index, const char *str);

  // Builds the entry list. Entries can't be added after this. False if an index was added twice.
  bool finalize();
  bool finalized() const { return this->finalized_; }

  OD_entry_t *find(uint16_t index) { return OD_find(&this->od_, index); }
  OD_t *od() { return &this->od_; }

 protected:
  // an entry until finalize(): its object by position in vars_ / arrays_ / records_
  struct PendingEntry {
    uint16_t index;
    uint8_t sub_count;
    uint8_t type;
    uint16_t object;
  };

  bool can_add_(uint16_t index) const;

  std::vector<PendingEntry> pending_;  // released by finalize()
  std::vector<OD_obj_var_t> vars_;
  std::vector<OD_obj_array_t> arrays_;
  std::vector<OD_obj_record_t> records_;  // the subs of all records, each record's in a row
  std::vector<OD_entry_t> entries_;
  OD_t od_{0, nullptr};
  bool finalized_{false};
};

}  // namespace esphome::canopen

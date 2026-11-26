#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

#include "esphome/core/defines.h"  // before CANopenNode: clang-tidy gets the CO_CONFIG_* values from it
#include "esphome/core/helpers.h"

#define OD_DEFINITION
// Use the library search path: quoted includes match our canopen.h on Windows.
#include <CANopen.h>

namespace esphome::canopen {

// Attributes of OD entries (CANopenNode's ODA_* values)
constexpr uint8_t OD_READ = ODA_SDO_R;
constexpr uint8_t OD_WRITE = ODA_SDO_W;
constexpr uint8_t OD_READ_WRITE = ODA_SDO_RW;

// Object dictionary built at runtime (CO_MULTIPLE_OD). Entries are collected with add_*() and turned into the sorted
// OD_entry_t list by finalize(), which copies them into arrays of their exact size and frees what was collected. The
// value storage is owned by the caller and must outlive the OD. All allocation happens in setup.
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
  bool finalized() const { return this->od_.list != nullptr; }

  OD_entry_t *find(uint16_t index) { return OD_find(&this->od_, index); }
  OD_t *od() { return &this->od_; }

 protected:
  // an entry until finalize(): its object by position in the vars / arrays / records collected so far
  struct PendingEntry {
    uint16_t index;
    uint8_t sub_count;
    uint8_t type;
    uint16_t object;
  };
  // what add_*() collects until finalize()
  struct Pending {
    std::vector<PendingEntry> entries;
    std::vector<OD_obj_var_t> vars;
    std::vector<OD_obj_array_t> arrays;
    std::vector<OD_obj_record_t> records;  // the subs of all records, each record's in a row
  };

  // nullptr once finalized: CANopenNode then holds pointers into the arrays below
  Pending *pending_(uint16_t index);

  std::unique_ptr<Pending> pending_storage_;
  FixedVector<OD_obj_var_t> vars_;
  FixedVector<OD_obj_array_t> arrays_;
  FixedVector<OD_obj_record_t> records_;
  FixedVector<OD_entry_t> entries_;
  OD_t od_{0, nullptr};
};

}  // namespace esphome::canopen

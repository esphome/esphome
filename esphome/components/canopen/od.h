#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <vector>

#include "esphome/core/helpers.h"

#include "od_handler.h"  // CANopenNode's CANopen.h, after esphome/core/defines.h

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

  // --- Handlers (od_handler.h, od_handler.cpp) ---
  // Calls handler->od_read() / od_write() (an OdObject subclass) for every access to that entry.
  template<typename T> void set_object(uint16_t index, T *handler) {
    static_assert(std::is_base_of_v<OdObject, T>, "OD handlers derive from canopen::OdObject");
    this->set_extension_(index, OD_extension_t{handler, &read_trampoline<T>, &write_trampoline<T>, {}});
  }
  // write() and read() transfer the value's bytes exactly: a fixed-size entry takes exactly its size. CANopenNode's
  // ODA_STR string handling (shorter writes, reads up to the terminator) isn't applied; the OD attributes don't
  // include it.
  // Writes a value through the entry's handler, as an SDO download does. Needs write access. After finalize().
  OdResult write(uint16_t index, uint8_t sub, const void *data, size_t len);
  // Reads a value through the entry's handler, as an SDO upload does, into data (max_len bytes); *len is the number of
  // bytes read. Needs read access. DATA_LONG if the value doesn't fit. A handler may have side effects (e.g. consume
  // what it returns), also when the value doesn't fit. After finalize().
  OdResult read(uint16_t index, uint8_t sub, void *data, size_t max_len, size_t *len);
  // Size in bytes of an entry's value, without running its handler; 0 if it has no fixed size (a domain) or doesn't
  // exist, and before finalize().
  size_t value_size(uint16_t index, uint8_t sub);

 protected:
  // an entry until finalize(): its object and extension by position in the vars / arrays / records / extensions
  // collected so far
  struct PendingEntry {
    uint16_t index;
    uint8_t sub_count;
    uint8_t type;
    uint16_t object;
    uint16_t extension{0};  // position + 1, 0 = none
  };
  // what add_*() collects until finalize()
  struct Pending {
    std::vector<PendingEntry> entries;
    std::vector<OD_obj_var_t> vars;
    std::vector<OD_obj_array_t> arrays;
    std::vector<OD_obj_record_t> records;  // the subs of all records, each record's in a row
    // set_object() calls with their entry's index: finalize() links the entries to them, so the order of add_*() and
    // set_object() doesn't matter
    struct PendingExtension {
      uint16_t index;
      OD_extension_t extension;
    };
    std::vector<PendingExtension> extensions;
    // set by set_object(), so a node without handlers doesn't link attach_handlers_()
    bool (ObjectDictionary::*attach)(std::vector<PendingEntry> &){nullptr};
  };

  // nullptr once finalized: CANopenNode then holds pointers into the arrays below
  Pending *pending_(uint16_t index);

  // --- Handlers (od_handler.cpp) ---
  void set_extension_(uint16_t index, const OD_extension_t &extension);
  // finalize(): links the sorted entries to their handlers; false for a handler without an entry or a second handler
  bool attach_handlers_(std::vector<PendingEntry> &entries);
  // the entry's sub for an access; DEVICE_STATE before finalize()
  OdResult get_sub_(uint16_t index, uint8_t sub, OD_IO_t *io);
  template<typename T>
  static ODR_t read_trampoline(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *count_read) {
    size_t done = 0;
    auto ret = static_cast<T *>(stream->object)->od_read(OdStream(stream), buf, count, &done);
    *count_read = done;
    return static_cast<ODR_t>(ret);
  }
  template<typename T>
  static ODR_t write_trampoline(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *count_written) {
    size_t done = 0;
    auto ret = static_cast<T *>(stream->object)->od_write(OdStream(stream), buf, count, &done);
    *count_written = done;
    return static_cast<ODR_t>(ret);
  }

  std::unique_ptr<Pending> pending_storage_;
  FixedVector<OD_obj_var_t> vars_;
  FixedVector<OD_obj_array_t> arrays_;
  FixedVector<OD_obj_record_t> records_;
  FixedVector<OD_extension_t> extensions_;
  FixedVector<OD_entry_t> entries_;
  OD_t od_{0, nullptr};
};

}  // namespace esphome::canopen

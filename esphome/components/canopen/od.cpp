#include "od.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen.od");

static OD_attr_t multi_byte(OD_attr_t attr, OD_size_t len) { return len >= 2 ? attr | ODA_MB : attr; }

ObjectDictionary::Pending *ObjectDictionary::pending_(uint16_t index) {
  if (this->finalized()) {
    ESP_LOGE(TAG, "OD 0x%04x added after init, ignored", index);
    return nullptr;
  }
  if (!this->pending_storage_)
    this->pending_storage_ = std::make_unique<Pending>();
  return this->pending_storage_.get();
}

void ObjectDictionary::add_var(uint16_t index, OD_attr_t attr, void *data, OD_size_t len) {
  Pending *p = this->pending_(index);
  if (p == nullptr)
    return;
  p->entries.push_back(PendingEntry{index, 1, ODT_VAR, static_cast<uint16_t>(p->vars.size()), 0});
  p->vars.push_back(OD_obj_var_t{data, multi_byte(attr, len), len});
}

void ObjectDictionary::add_array(uint16_t index, OD_attr_t attr, uint8_t *count, void *data, OD_size_t elem_len,
                                 uint8_t n) {
  Pending *p = this->pending_(index);
  if (p == nullptr)
    return;
  p->entries.push_back(
      PendingEntry{index, static_cast<uint8_t>(n + 1), ODT_ARR, static_cast<uint16_t>(p->arrays.size()), 0});
  p->arrays.push_back(OD_obj_array_t{count, data, ODA_SDO_R, multi_byte(attr, elem_len), elem_len, elem_len});
}

void ObjectDictionary::add_record(uint16_t index, const RecordSub *subs, size_t count) {
  Pending *p = this->pending_(index);
  if (p == nullptr)
    return;
  p->entries.push_back(
      PendingEntry{index, static_cast<uint8_t>(count), ODT_REC, static_cast<uint16_t>(p->records.size()), 0});
  for (size_t i = 0; i < count; i++) {
    const RecordSub &s = subs[i];
    p->records.push_back(OD_obj_record_t{s.data, s.sub, multi_byte(s.attr, s.len), s.len});
  }
}

void ObjectDictionary::add_string(uint16_t index, const char *str) {
  Pending *p = this->pending_(index);
  if (p == nullptr)
    return;
  OD_size_t len = std::strlen(str);
  p->entries.push_back(PendingEntry{index, 1, ODT_VAR, static_cast<uint16_t>(p->vars.size()), 0});
  // no ODA_MB: a string has no byte order
  p->vars.push_back(OD_obj_var_t{const_cast<char *>(str), ODA_SDO_R, len > 0 ? len : 1});
}

template<typename T> static void copy_exact(FixedVector<T> &to, const std::vector<T> &from) {
  to.init(from.size());
  for (const T &item : from)
    to.push_back(item);
}

bool ObjectDictionary::finalize() {
  // an OD without entries still gets the blank last element
  if (!this->pending_storage_)
    this->pending_storage_ = std::make_unique<Pending>();
  auto &pending = this->pending_storage_->entries;
  std::sort(pending.begin(), pending.end(),
            [](const PendingEntry &a, const PendingEntry &b) { return a.index < b.index; });
  bool ok = true;
  for (size_t i = 1; i < pending.size(); i++) {
    if (pending[i].index == pending[i - 1].index) {
      ESP_LOGE(TAG, "OD 0x%04x added twice", pending[i].index);
      ok = false;
    }
  }
  copy_exact(this->vars_, this->pending_storage_->vars);
  copy_exact(this->arrays_, this->pending_storage_->arrays);
  copy_exact(this->records_, this->pending_storage_->records);
  copy_exact(this->extensions_, this->pending_storage_->extensions);
  // OD_find() expects a blank last element
  this->entries_.init(pending.size() + 1);
  for (const auto &p : pending) {
    void *obj;
    if (p.type == ODT_VAR) {
      obj = &this->vars_[p.object];
    } else if (p.type == ODT_ARR) {
      obj = &this->arrays_[p.object];
    } else {
      obj = &this->records_[p.object];
    }
    OD_extension_t *ext = p.extension ? &this->extensions_[p.extension - 1] : nullptr;
    this->entries_.push_back(OD_entry_t{p.index, p.sub_count, p.type, obj, ext});
  }
  this->entries_.push_back(OD_entry_t{0, 0, 0, nullptr, nullptr});
  this->pending_storage_.reset();
  this->od_.size = this->entries_.size() - 1;
  this->od_.list = this->entries_.begin();
  return ok;
}

}  // namespace esphome::canopen

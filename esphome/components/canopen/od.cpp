#include "od.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen.od");

static OD_attr_t multi_byte(OD_attr_t attr, OD_size_t len) { return len >= 2 ? attr | ODA_MB : attr; }

bool ObjectDictionary::can_add_(uint16_t index) const {
  // nothing may grow after finalize(): CANopenNode holds pointers into the containers
  if (this->finalized_) {
    ESP_LOGE(TAG, "OD 0x%04x added after init, ignored", index);
    return false;
  }
  return true;
}

void ObjectDictionary::add_var(uint16_t index, OD_attr_t attr, void *data, OD_size_t len) {
  if (!this->can_add_(index))
    return;
  this->pending_.push_back(PendingEntry{index, 1, ODT_VAR, static_cast<uint16_t>(this->vars_.size())});
  this->vars_.push_back(OD_obj_var_t{data, multi_byte(attr, len), len});
}

void ObjectDictionary::add_array(uint16_t index, OD_attr_t attr, uint8_t *count, void *data, OD_size_t elem_len,
                                 uint8_t n) {
  if (!this->can_add_(index))
    return;
  this->pending_.push_back(
      PendingEntry{index, static_cast<uint8_t>(n + 1), ODT_ARR, static_cast<uint16_t>(this->arrays_.size())});
  this->arrays_.push_back(OD_obj_array_t{count, data, ODA_SDO_R, multi_byte(attr, elem_len), elem_len, elem_len});
}

void ObjectDictionary::add_record(uint16_t index, const RecordSub *subs, size_t count) {
  if (!this->can_add_(index))
    return;
  this->pending_.push_back(
      PendingEntry{index, static_cast<uint8_t>(count), ODT_REC, static_cast<uint16_t>(this->records_.size())});
  for (size_t i = 0; i < count; i++) {
    const RecordSub &s = subs[i];
    this->records_.push_back(OD_obj_record_t{s.data, s.sub, multi_byte(s.attr, s.len), s.len});
  }
}

void ObjectDictionary::add_string(uint16_t index, const char *str) {
  if (!this->can_add_(index))
    return;
  OD_size_t len = std::strlen(str);
  this->pending_.push_back(PendingEntry{index, 1, ODT_VAR, static_cast<uint16_t>(this->vars_.size())});
  // no ODA_MB: a string has no byte order
  this->vars_.push_back(OD_obj_var_t{const_cast<char *>(str), ODA_SDO_R, len > 0 ? len : 1});
}

bool ObjectDictionary::finalize() {
  auto &pending = this->pending_;
  std::sort(pending.begin(), pending.end(),
            [](const PendingEntry &a, const PendingEntry &b) { return a.index < b.index; });
  bool ok = true;
  for (size_t i = 1; i < pending.size(); i++) {
    if (pending[i].index == pending[i - 1].index) {
      ESP_LOGE(TAG, "OD 0x%04x added twice", pending[i].index);
      ok = false;
    }
  }
  // done growing: drop the spare capacity before taking pointers
  this->vars_.shrink_to_fit();
  this->arrays_.shrink_to_fit();
  this->records_.shrink_to_fit();
  // OD_find() expects a blank last element
  this->entries_.reserve(pending.size() + 1);
  for (const auto &p : pending) {
    void *obj;
    if (p.type == ODT_VAR) {
      obj = &this->vars_[p.object];
    } else if (p.type == ODT_ARR) {
      obj = &this->arrays_[p.object];
    } else {
      obj = &this->records_[p.object];
    }
    this->entries_.push_back(OD_entry_t{p.index, p.sub_count, p.type, obj, nullptr});
  }
  this->entries_.push_back(OD_entry_t{0, 0, 0, nullptr, nullptr});
  std::vector<PendingEntry>().swap(pending);
  this->od_.size = this->entries_.size() - 1;
  this->od_.list = this->entries_.data();
  this->finalized_ = true;
  return ok;
}

}  // namespace esphome::canopen

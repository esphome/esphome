#include "esphome/core/defines.h"

#ifdef USE_BINARY_STORAGE_INPLACE_KV

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "esphome/components/binary_storage/inplace_kv.h"
#include "esphome/components/storage/storage.h"

// Exercises the in-place KeyValueStorage on the host over a mock byte medium: the get/set/erase/has/
// get_size/format contract, and -- the substance -- recovery from a torn write and a torn compaction.
// The real FRAM/MRAM backing is bus hardware, so this uses an in-memory RawStorage that advertises the
// same in-place (no-erase) capability and can inject a write fault to simulate power loss.

namespace esphome::binary_storage::testing {

using storage::StorageError;

class FakeByteMedium : public storage::RawStorage {
 public:
  explicit FakeByteMedium(size_t n) : data_(n, 0xCC) {}  // 0xCC = uninitialized medium

  StorageError get_info(storage::StorageInfo *info) override {
    *info = storage::StorageInfo{};
    return StorageError::OK;
  }
  StorageError read(uint64_t off, uint8_t *buf, size_t len, size_t *got) override {
    *got = 0;
    if (off + len > this->data_.size())
      return StorageError::INVALID_ARGS;
    std::memcpy(buf, &this->data_[off], len);
    *got = len;
    return StorageError::OK;
  }
  StorageError write(uint64_t off, const uint8_t *buf, size_t len, size_t *put) override {
    *put = 0;
    if (off + len > this->data_.size())
      return StorageError::INVALID_ARGS;
    if (this->fail_after >= 0 && this->writes >= this->fail_after)
      return StorageError::WRITE_ERROR;  // simulated power loss
    ++this->writes;
    std::memcpy(&this->data_[off], buf, len);
    *put = len;
    return StorageError::OK;
  }
  StorageError erase(uint64_t, size_t) override { return StorageError::NOT_SUPPORTED; }  // in-place: no erase
  StorageError format() override {
    std::fill(this->data_.begin(), this->data_.end(), 0xCC);
    return StorageError::OK;
  }
  void get_raw_geometry(storage::RawGeometry *out) const override {
    *out = storage::RawGeometry{};
    out->capacity = this->data_.size();
    out->write_page = 1;
    out->caps = 0;  // no RAW_WRITE_NEEDS_ERASE -> in-place medium
  }

  long fail_after{-1};
  long writes{0};

 private:
  std::vector<uint8_t> data_;
};

namespace {

void setup_store(InplaceKVStore &kv, FakeByteMedium *medium, uint64_t size) {
  kv.set_device(medium);
  kv.set_window(0, size);
}

}  // namespace

// ---------------------------------------------------------------------------
// Contract
// ---------------------------------------------------------------------------

TEST(InplaceKV, AdvertisesKeyValueType) {
  FakeByteMedium m(4096);
  InplaceKVStore kv;
  setup_store(kv, &m, 4096);
  EXPECT_EQ(kv.get_storage_type(), storage::StorageType::KEY_VALUE);
}

TEST(InplaceKV, RefusesEraseMedium) {
  // A medium that needs erase must be rejected -- this store is only correct in place.
  class NeedsErase : public FakeByteMedium {
   public:
    using FakeByteMedium::FakeByteMedium;
    void get_raw_geometry(storage::RawGeometry *out) const override {
      *out = storage::RawGeometry{};
      out->capacity = 4096;
      out->caps = storage::RAW_WRITE_NEEDS_ERASE;
    }
  } m(4096);
  InplaceKVStore kv;
  setup_store(kv, &m, 4096);
  EXPECT_EQ(kv.ensure_initialized(), StorageError::NOT_SUPPORTED);
}

TEST(InplaceKV, SetGetEraseContract) {
  FakeByteMedium m(4096);
  InplaceKVStore kv;
  setup_store(kv, &m, 4096);
  ASSERT_EQ(kv.ensure_initialized(), StorageError::OK);

  const uint32_t key = 4231u;
  const uint8_t value[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01};
  uint8_t buf[64];
  size_t got = 0;
  size_t sz = 0;

  EXPECT_FALSE(kv.has(key));
  EXPECT_EQ(kv.get(key, buf, sizeof(buf), &got), StorageError::NOT_FOUND);

  ASSERT_EQ(kv.set(key, value, sizeof(value)), StorageError::OK);
  EXPECT_TRUE(kv.has(key));
  ASSERT_EQ(kv.get_size(key, &sz), StorageError::OK);
  EXPECT_EQ(sz, sizeof(value));

  ASSERT_EQ(kv.get(key, buf, sizeof(buf), &got), StorageError::OK);
  EXPECT_EQ(got, sizeof(value));
  EXPECT_EQ(0, std::memcmp(buf, value, sizeof(value)));

  uint8_t small[2];
  EXPECT_EQ(kv.get(key, small, sizeof(small), &got), StorageError::INVALID_ARGS);
  EXPECT_EQ(got, 0u);

  EXPECT_EQ(kv.erase(key), StorageError::OK);
  EXPECT_FALSE(kv.has(key));
  EXPECT_EQ(kv.erase(key), StorageError::OK);  // idempotent
}

TEST(InplaceKV, OverwriteLastWins) {
  FakeByteMedium m(4096);
  InplaceKVStore kv;
  setup_store(kv, &m, 4096);
  const uint8_t a[] = {1, 2, 3};
  const uint8_t b[] = {9, 9};
  ASSERT_EQ(kv.set(5u, a, sizeof(a)), StorageError::OK);
  ASSERT_EQ(kv.set(5u, b, sizeof(b)), StorageError::OK);
  uint8_t buf[8];
  size_t got = 0;
  ASSERT_EQ(kv.get(5u, buf, sizeof(buf), &got), StorageError::OK);
  EXPECT_EQ(got, sizeof(b));
  EXPECT_EQ(0, std::memcmp(buf, b, sizeof(b)));
}

TEST(InplaceKV, FormatWipes) {
  FakeByteMedium m(4096);
  InplaceKVStore kv;
  setup_store(kv, &m, 4096);
  const uint8_t v[] = {1};
  ASSERT_EQ(kv.set(1u, v, 1), StorageError::OK);
  ASSERT_EQ(kv.set(2u, v, 1), StorageError::OK);
  ASSERT_EQ(kv.format(), StorageError::OK);
  EXPECT_FALSE(kv.has(1u));
  EXPECT_FALSE(kv.has(2u));
}

// ---------------------------------------------------------------------------
// Compaction (small window forces it)
// ---------------------------------------------------------------------------

TEST(InplaceKV, SurvivesManyCompactions) {
  FakeByteMedium m(256);
  InplaceKVStore kv;
  setup_store(kv, &m, 256);
  for (int i = 0; i < 200; i++) {
    uint8_t v[4] = {(uint8_t) i, (uint8_t) (i >> 8), 0xAB, 0xCD};
    ASSERT_EQ(kv.set(42u, v, 4), StorageError::OK) << "iteration " << i;
  }
  uint8_t buf[8];
  size_t got = 0;
  ASSERT_EQ(kv.get(42u, buf, sizeof(buf), &got), StorageError::OK);
  const uint8_t expect[4] = {199 & 0xFF, (199 >> 8) & 0xFF, 0xAB, 0xCD};
  EXPECT_EQ(got, 4u);
  EXPECT_EQ(0, std::memcmp(buf, expect, 4));
}

TEST(InplaceKV, MultiKeyCompaction) {
  FakeByteMedium m(512);
  InplaceKVStore kv;
  setup_store(kv, &m, 512);
  for (int r = 0; r < 50; r++) {
    for (uint32_t k = 1; k <= 3; k++) {
      uint8_t v[3] = {(uint8_t) k, (uint8_t) r, 0xEE};
      ASSERT_EQ(kv.set(k, v, 3), StorageError::OK);
    }
  }
  uint8_t buf[8];
  size_t got = 0;
  for (uint32_t k = 1; k <= 3; k++) {
    ASSERT_EQ(kv.get(k, buf, sizeof(buf), &got), StorageError::OK);
    const uint8_t expect[3] = {(uint8_t) k, 49, 0xEE};
    EXPECT_EQ(got, 3u);
    EXPECT_EQ(0, std::memcmp(buf, expect, 3)) << "key " << k;
  }
}

// ---------------------------------------------------------------------------
// Power-loss recovery: a fresh store re-reads the medium (remount).
// ---------------------------------------------------------------------------

TEST(InplaceKV, TornWriteBeforeCommitKeepsOldValue) {
  FakeByteMedium m(4096);
  {
    InplaceKVStore kv;
  setup_store(kv, &m, 4096);
    const uint8_t a[] = {7, 7, 7, 7};
    ASSERT_EQ(kv.set(5u, a, 4), StorageError::OK);
    m.fail_after = m.writes + 2;  // header + value written, fault before the commit marker
    const uint8_t b[] = {5, 5};
    kv.set(5u, b, 2);  // interrupted
    m.fail_after = -1;
  }
  InplaceKVStore kv2;
  setup_store(kv2, &m, 4096);  // remount
  uint8_t buf[8];
  size_t got = 0;
  ASSERT_EQ(kv2.get(5u, buf, sizeof(buf), &got), StorageError::OK);
  EXPECT_EQ(got, 4u);  // old value survived; torn entry ignored
}

TEST(InplaceKV, TornWriteAfterCommitTakesNewValue) {
  FakeByteMedium m(4096);
  {
    InplaceKVStore kv;
  setup_store(kv, &m, 4096);
    const uint8_t a[] = {7, 7, 7, 7};
    ASSERT_EQ(kv.set(5u, a, 4), StorageError::OK);
    m.fail_after = m.writes + 3;  // header + value + commit succeed, fault before clearing the old
    const uint8_t b[] = {5, 5};
    kv.set(5u, b, 2);
    m.fail_after = -1;
  }
  InplaceKVStore kv2;
  setup_store(kv2, &m, 4096);
  uint8_t buf[8];
  size_t got = 0;
  ASSERT_EQ(kv2.get(5u, buf, sizeof(buf), &got), StorageError::OK);
  EXPECT_EQ(got, 2u);  // new value wins (last-wins)
}

TEST(InplaceKV, TornCompactionRecovers) {
  FakeByteMedium m(256);
  const uint8_t untouched[] = {1, 2, 3, 4};
  {
    InplaceKVStore kv;
  setup_store(kv, &m, 256);
    ASSERT_EQ(kv.set(6u, untouched, 4), StorageError::OK);  // a key we never touch again
    // Hammer key 5 to force a compaction, faulting partway through the rebuild.
    m.fail_after = m.writes + 2;
    for (int i = 0; i < 60; i++) {
      uint8_t v[4] = {(uint8_t) i, 1, 2, 3};
      if (kv.set(5u, v, 4) != StorageError::OK)
        break;
    }
    m.fail_after = -1;
  }
  InplaceKVStore kv2;
  setup_store(kv2, &m, 256);  // remount
  uint8_t buf[8];
  size_t got = 0;
  // The untouched key must be intact regardless of when the compaction was cut.
  ASSERT_EQ(kv2.get(6u, buf, sizeof(buf), &got), StorageError::OK);
  EXPECT_EQ(got, 4u);
  EXPECT_EQ(0, std::memcmp(buf, untouched, 4));
  EXPECT_TRUE(kv2.has(5u));  // hammered key still present
}

}  // namespace esphome::binary_storage::testing

#endif  // USE_BINARY_STORAGE_INPLACE_KV

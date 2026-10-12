#include <gtest/gtest.h>

#include "esphome/components/ota/ota_backend_factory.h"

namespace esphome::ota::testing {

TEST(UpdateLock, SecondHolderIsRefused) {
  UpdateLock first;
  UpdateLock second;
  EXPECT_TRUE(first.try_acquire());
  EXPECT_FALSE(second.try_acquire());
  first.release();
  EXPECT_TRUE(second.try_acquire());
}

TEST(UpdateLock, AcquireIsIdempotentForTheHolder) {
  UpdateLock lock;
  EXPECT_TRUE(lock.try_acquire());
  EXPECT_TRUE(lock.try_acquire());
  lock.release();
  UpdateLock other;
  EXPECT_TRUE(other.try_acquire());
}

TEST(UpdateLock, ReleaseByNonHolderKeepsLock) {
  UpdateLock holder;
  UpdateLock other;
  EXPECT_TRUE(holder.try_acquire());
  other.release();
  EXPECT_FALSE(other.try_acquire());
}

TEST(UpdateLock, DestructorReleases) {
  {
    UpdateLock scoped;
    EXPECT_TRUE(scoped.try_acquire());
  }
  UpdateLock next;
  EXPECT_TRUE(next.try_acquire());
}

TEST(UpdateLock, IsHeldTracksOwnership) {
  UpdateLock holder;
  UpdateLock other;
  EXPECT_FALSE(holder.is_held());
  EXPECT_TRUE(holder.try_acquire());
  EXPECT_TRUE(holder.is_held());
  EXPECT_FALSE(other.try_acquire());
  EXPECT_FALSE(other.is_held());
}

class TestUpdateLock : public UpdateLock {
 public:
  using UpdateLock::force_release;
};

TEST(UpdateLock, HoldUntilRebootOutlivesHolder) {
  {
    UpdateLock committed;
    EXPECT_TRUE(committed.try_acquire());
    committed.hold_until_reboot();
    EXPECT_FALSE(committed.is_held());
  }
  UpdateLock next;
  EXPECT_FALSE(next.try_acquire());
  TestUpdateLock::force_release();
  EXPECT_TRUE(next.try_acquire());
}

TEST(UpdateLock, SecondBackendBeginIsBusy) {
  auto first = make_ota_backend();
  auto second = make_ota_backend();
  ASSERT_EQ(first->begin(1024), OTA_RESPONSE_OK);
  EXPECT_EQ(second->begin(1024), OTA_RESPONSE_ERROR_UPDATE_BUSY);
  // Native OTA calls abort() after a refused begin().
  second->abort();
  uint8_t data[16] = {};
  EXPECT_EQ(first->write(data, sizeof(data)), OTA_RESPONSE_OK);
  first->abort();
  first.reset();
  EXPECT_EQ(second->begin(1024), OTA_RESPONSE_OK);
  second->abort();
}

}  // namespace esphome::ota::testing

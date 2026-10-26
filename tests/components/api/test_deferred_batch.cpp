#include <gtest/gtest.h>

#include "esphome/components/api/api_connection.h"

namespace esphome::api::testing {

class DeferredBatchTest : public ::testing::Test {
 protected:
  using Batch = DeferredBatch;

  // Only the pointers are compared, so the entities are never dereferenced
  EntityBase *entity_(size_t n) { return reinterpret_cast<EntityBase *>(&this->storage_[n]); }

  static std::vector<EntityBase *> entities(const Batch &batch) {
    std::vector<EntityBase *> out;
    for (size_t i = 0; i < batch.size(); i++)
      out.push_back(batch[i].entity);
    return out;
  }

  static constexpr uint16_t STATE = 1;
  static constexpr uint16_t PING = 2;
  uint32_t storage_[4]{};
};

TEST_F(DeferredBatchTest, FrontItemKeepsTheQueuedOrder) {
  Batch batch;
  batch.add_item(this->entity_(0), STATE, 8);
  batch.add_item(this->entity_(1), STATE, 8);
  batch.add_item(this->entity_(2), STATE, 8);

  batch.add_item_front(nullptr, PING, 4);

  // A swap would have moved the oldest item to the back
  EXPECT_EQ(entities(batch),
            (std::vector<EntityBase *>{nullptr, this->entity_(0), this->entity_(1), this->entity_(2)}));
  EXPECT_EQ(batch[0].message_type, PING);
}

TEST_F(DeferredBatchTest, FrontItemIntoAnEmptyBatch) {
  Batch batch;
  batch.add_item_front(nullptr, PING, 4);
  ASSERT_EQ(batch.size(), 1u);
  EXPECT_EQ(batch[0].message_type, PING);
}

TEST_F(DeferredBatchTest, StatesAreQueuedOnce) {
  Batch batch;
  batch.add_item(this->entity_(0), STATE, 8);
  batch.add_item(this->entity_(0), STATE, 8);
  EXPECT_EQ(batch.size(), 1u);
}

TEST_F(DeferredBatchTest, ContainsMatchesEntityAndMessageType) {
  Batch batch;
  batch.add_item(this->entity_(0), STATE, 8);
  EXPECT_TRUE(batch.contains(this->entity_(0), STATE));
  EXPECT_FALSE(batch.contains(this->entity_(0), PING));
  EXPECT_FALSE(batch.contains(this->entity_(1), STATE));
}

}  // namespace esphome::api::testing

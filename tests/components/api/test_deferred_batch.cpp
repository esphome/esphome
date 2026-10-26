#include <gtest/gtest.h>

#include "esphome/components/api/api_deferred_batch.h"

namespace esphome::api::testing {

class DeferredBatchTest : public ::testing::Test {
 protected:
  static constexpr uint16_t STATE = 1;
  static constexpr uint16_t PING = 2;
  EntityBase entities_[3];
  DeferredBatch batch_;
};

TEST_F(DeferredBatchTest, FrontItemKeepsTheQueuedOrder) {
  for (auto &entity : this->entities_)
    this->batch_.add_item(&entity, STATE, 8);

  this->batch_.add_item_front(nullptr, PING, 4);

  // A swap would have moved the oldest item to the back
  std::vector<EntityBase *> order;
  for (size_t i = 0; i < this->batch_.size(); i++)
    order.push_back(this->batch_[i].entity);
  EXPECT_EQ(order, (std::vector<EntityBase *>{nullptr, &this->entities_[0], &this->entities_[1], &this->entities_[2]}));
  EXPECT_EQ(this->batch_[0].message_type, PING);
}

TEST_F(DeferredBatchTest, FrontItemIntoAnEmptyBatch) {
  this->batch_.add_item_front(nullptr, PING, 4);
  ASSERT_EQ(this->batch_.size(), 1u);
  EXPECT_EQ(this->batch_[0].message_type, PING);
}

TEST_F(DeferredBatchTest, StatesAreQueuedOnce) {
  this->batch_.add_item(&this->entities_[0], STATE, 8);
  this->batch_.add_item(&this->entities_[0], STATE, 8);
  EXPECT_EQ(this->batch_.size(), 1u);
}

TEST_F(DeferredBatchTest, ContainsMatchesEntityAndMessageType) {
  this->batch_.add_item(&this->entities_[0], STATE, 8);
  EXPECT_TRUE(this->batch_.contains(&this->entities_[0], STATE));
  EXPECT_FALSE(this->batch_.contains(&this->entities_[0], PING));
  EXPECT_FALSE(this->batch_.contains(&this->entities_[1], STATE));
}

}  // namespace esphome::api::testing

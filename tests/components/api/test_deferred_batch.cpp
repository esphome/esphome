#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/api/api_connection.h"

namespace esphome::api::testing {

// Reads the batch type; APIConnection is final, so this is a friend rather than a subclass.
class DeferredBatchAccess {
 public:
  using Batch = APIConnection::DeferredBatch;
};

static constexpr uint16_t ANY_TYPE = 0;

TEST(DeferredBatch, FrontItemKeepsTheWaitingItemsInOrder) {
  EntityBase queued_first;
  EntityBase queued_second;
  DeferredBatchAccess::Batch batch;
  batch.add_item(&queued_first, ANY_TYPE, 0);
  batch.add_item(&queued_second, ANY_TYPE, 0);

  // A keepalive ping that finds the send buffer full is queued at the front. The waiting items must keep
  // their order behind it, or a newer item reaches the client before an older one.
  batch.add_item_front(nullptr, PingRequest::MESSAGE_TYPE, PingRequest::ESTIMATED_SIZE);

  ASSERT_EQ(batch.size(), 3u);
  EXPECT_EQ(batch[0].message_type, PingRequest::MESSAGE_TYPE);
  EXPECT_EQ(batch[1].entity, &queued_first);
  EXPECT_EQ(batch[2].entity, &queued_second);
}

TEST(DeferredBatch, FrontItemInAnEmptyBatchIsTheOnlyItem) {
  DeferredBatchAccess::Batch batch;

  // Nothing waits, so nothing moves: the ping is the whole batch.
  batch.add_item_front(nullptr, PingRequest::MESSAGE_TYPE, PingRequest::ESTIMATED_SIZE);

  ASSERT_EQ(batch.size(), 1u);
  EXPECT_EQ(batch[0].message_type, PingRequest::MESSAGE_TYPE);
}

}  // namespace esphome::api::testing

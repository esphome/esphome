#include <gtest/gtest.h>

#include "esphome/core/entity_base.h"

#include <type_traits>
#include <utility>

namespace esphome::core::testing {

// External components return get_name() from `const StringRef &` getters, so it must stay a reference
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static_assert(std::is_same_v<decltype(std::declval<const EntityBase &>().get_name()), const StringRef &>);
#pragma GCC diagnostic pop

class TestEntity : public EntityBase {
 public:
  void configure(const char *name, uint32_t object_id_hash) { this->configure_entity_(name, object_id_hash, 0); }
};

TEST(EntityBaseName, ZeroHashIsComputedFromTheName) {
  TestEntity entity;
  entity.configure("Living Room Temp", 0);
  EXPECT_EQ(entity.get_object_id_hash(), fnv1_hash_object_id("Living Room Temp", 16));
}

TEST(EntityBaseName, PrecomputedHashIsKept) {
  TestEntity entity;
  entity.configure("Living Room Temp", 0x1234);
  EXPECT_EQ(entity.get_object_id_hash(), 0x1234u);
}

}  // namespace esphome::core::testing

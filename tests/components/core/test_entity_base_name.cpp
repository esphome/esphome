#include <gtest/gtest.h>

#include "esphome/core/entity_base.h"

#include <cstring>
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

// The values come from the Python fnv1_hash_object_id() test table. Reading through the byte
// reader is the path a name in flash takes on ESP8266.
TEST(EntityBaseName, HashThroughByteReaderMatchesPlainHash) {
  const char *name = "My Sensor Name";
  const size_t len = strlen(name);
  const uint32_t expected = 0x2760962A;
  EXPECT_EQ(fnv1_hash_object_id(name, len), expected);
  EXPECT_EQ(fnv1_hash_object_id(len, [name](size_t i) { return name[i]; }), expected);
  EXPECT_EQ(fnv1_hash_object_id(0, [](size_t) { return 'x'; }), 0x811C9DC5u);
}

TEST(EntityBaseName, PrecomputedHashIsKept) {
  TestEntity entity;
  entity.configure("Living Room Temp", 0x1234);
  EXPECT_EQ(entity.get_object_id_hash(), 0x1234u);
}

// The getters return {} when the feature define is absent; get_*_to() must still hand back "", not null
TEST(EntityBaseStrings, DefaultProgmemStringRefIsEmptyNotNull) {
  constexpr ProgmemStringRef ref{};
  ASSERT_NE(ref.progmem_ptr(), nullptr);
  EXPECT_STREQ(ref.progmem_ptr(), "");
  EXPECT_TRUE(ref.empty());
}

TEST(EntityBaseStrings, UnsetIconAndDeviceClassAreEmpty) {
  TestEntity entity;
  entity.configure("Living Room Temp", 0x1234);
  EXPECT_TRUE(entity.get_icon_progmem().empty());
  EXPECT_TRUE(entity.get_device_class_progmem().empty());
  char icon_buf[MAX_ICON_LENGTH];
  char dc_buf[MAX_DEVICE_CLASS_LENGTH];
  EXPECT_STREQ(entity.get_icon_to(icon_buf), "");
  EXPECT_STREQ(entity.get_device_class_to(dc_buf), "");
}

}  // namespace esphome::core::testing

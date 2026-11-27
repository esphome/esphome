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
  void configure(const char *name, uint32_t entity_key) { this->configure_entity_(name, entity_key, 0); }
  uint32_t legacy_hash() const {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return this->get_object_id_hash();
#pragma GCC diagnostic pop
  }
};

TEST(EntityBaseName, ZeroKeyIsComputedFromTheRawName) {
  TestEntity entity;
  entity.configure("Living Room Temp", 0);
  EXPECT_EQ(entity.get_entity_key(), 0x7E5CC74Fu);
}

TEST(EntityBaseName, PrecomputedKeyIsKept) {
  TestEntity entity;
  entity.configure("Living Room Temp", 0x1234);
  EXPECT_EQ(entity.get_entity_key(), 0x1234u);
}

// get_object_id_hash() keeps its legacy value, rebuilt from the name, whatever the key is
TEST(EntityBaseName, LegacyHashIsRebuiltFromTheName) {
  TestEntity entity;
  entity.configure("Living Room Temp", 0x1234);
  EXPECT_EQ(entity.legacy_hash(), 0x9D099819u);
}

// Named entities got their legacy hash from Python, which sanitizes per code point
TEST(EntityBaseName, LegacyHashOfUtf8NameIsPerCodePoint) {
  TestEntity entity;
  entity.configure("Température", 0);
  EXPECT_EQ(entity.get_entity_key(), 0x531A74AAu);
  EXPECT_EQ(entity.legacy_hash(), 0x965698F3u);
}

// The values come from the Python fnv1_hash_name() and fnv1_hash_object_id() test tables; Python
// has no per-byte form. Reading through the byte reader is the path a name in flash takes on ESP8266.
// Bytes >= 0x80 hash as unsigned, so the values do not depend on the signedness of char.
TEST(EntityBaseName, HashTable) {
  struct Case {
    const char *name;
    uint32_t raw;
    uint32_t legacy_per_byte;
    uint32_t legacy_per_code_point;
  };
  static constexpr Case CASES[] = {
      {"", 0x811C9DC5u, 0x811C9DC5u, 0x811C9DC5u},
      {"My Sensor Name", 0x8CEC6FB0u, 0x2760962Au, 0x2760962Au},
      // Per byte, each of the two UTF-8 bytes of 'é' becomes one underscore
      {"Température", 0x531A74AAu, 0x131A01F8u, 0x965698F3u},
  };
  for (const auto &c : CASES) {
    const size_t len = strlen(c.name);
    auto reader = [name = c.name](size_t i) { return name[i]; };
    EXPECT_EQ(fnv1_hash_bytes(c.name, len), c.raw) << c.name;
    EXPECT_EQ(fnv1_hash_bytes(len, reader), c.raw) << c.name;
    EXPECT_EQ(fnv1_hash_object_id(c.name, len, false), c.legacy_per_byte) << c.name;
    EXPECT_EQ(fnv1_hash_object_id(len, reader, false), c.legacy_per_byte) << c.name;
    EXPECT_EQ(fnv1_hash_object_id(c.name, len, true), c.legacy_per_code_point) << c.name;
    EXPECT_EQ(fnv1_hash_object_id(len, reader, true), c.legacy_per_code_point) << c.name;
  }
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

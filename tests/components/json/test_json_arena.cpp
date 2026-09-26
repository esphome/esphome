#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "esphome/components/json/json_util.h"

using esphome::json::JsonArena;
using esphome::json::JsonBuilder;

namespace {

constexpr size_t ALIGN = alignof(std::max_align_t);
constexpr size_t round_up(size_t n) { return (n + ALIGN - 1) & ~(ALIGN - 1); }

// Counts what the arena could not hold
struct Counting final : ArduinoJson::Allocator {
  int allocs{0};
  void *allocate(size_t n) override {
    this->allocs++;
    return malloc(n);  // NOLINT
  }
  void deallocate(void *p) override { free(p); }  // NOLINT
  void *reallocate(void *p, size_t n) override {
    this->allocs++;
    return realloc(p, n);  // NOLINT
  }
};

// Refuses everything, so a spill or a move sees the heap as exhausted
struct NoMemory final : ArduinoJson::Allocator {
  void *allocate(size_t) override { return nullptr; }
  void deallocate(void *) override {}
  void *reallocate(void *, size_t) override { return nullptr; }
};

template<size_t N> bool inside(const JsonArena<N> &arena, const void *p) {
  auto base = reinterpret_cast<uintptr_t>(&arena);
  auto addr = reinterpret_cast<uintptr_t>(p);
  return addr >= base && addr < base + sizeof(arena);
}

}  // namespace

TEST(JsonArena, BumpsAlignedInsideTheBuffer) {
  JsonArena<256> arena;
  auto *a = static_cast<uint8_t *>(arena.allocate(10));
  auto *b = static_cast<uint8_t *>(arena.allocate(10));
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(inside(arena, a));
  EXPECT_TRUE(inside(arena, b));
  EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % ALIGN, 0u);
  EXPECT_EQ(static_cast<size_t>(b - a), round_up(10));
  arena.deallocate(a);
  arena.deallocate(b);
}

TEST(JsonArena, SpillsToTheHeapWhenFull) {
  JsonArena<64> arena;
  void *a = arena.allocate(48);
  void *b = arena.allocate(48);
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(inside(arena, a));
  EXPECT_FALSE(inside(arena, b));
  std::memset(b, 'b', 48);
  arena.deallocate(b);  // routed to the heap; a mismatch would trip the sanitizer
  arena.deallocate(a);
}

TEST(JsonArena, NewestBlockGrowsAndShrinksInPlace) {
  JsonArena<256> arena;
  void *a = arena.allocate(16);
  std::memset(a, 'x', 16);
  EXPECT_EQ(arena.reallocate(a, 96), a);
  EXPECT_EQ(std::memcmp(a, "xxxxxxxxxxxxxxxx", 16), 0);
  EXPECT_EQ(arena.reallocate(a, 8), a);
  auto *next = static_cast<uint8_t *>(arena.allocate(8));
  EXPECT_EQ(static_cast<size_t>(next - static_cast<uint8_t *>(a)), round_up(8));
}

TEST(JsonArena, NewestBlockMovesToTheHeapAndFreesItsSpace) {
  JsonArena<64> arena;
  void *a = arena.allocate(32);
  std::memset(a, 'q', 32);
  void *moved = arena.reallocate(a, 200);
  ASSERT_NE(moved, nullptr);
  EXPECT_FALSE(inside(arena, moved));
  EXPECT_EQ(std::memcmp(moved, "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq", 32), 0);
  EXPECT_EQ(arena.allocate(16), a);  // the space it left is handed out again
  arena.deallocate(moved);
}

TEST(JsonArena, OlderBlockMovesToTheHeapKeepingItsBytes) {
  JsonArena<256> arena;
  void *a = arena.allocate(16);
  std::memset(a, 'a', 16);
  auto *b = static_cast<uint8_t *>(arena.allocate(16));
  void *moved = arena.reallocate(a, 64);
  ASSERT_NE(moved, nullptr);
  EXPECT_FALSE(inside(arena, moved));
  EXPECT_EQ(std::memcmp(moved, "aaaaaaaaaaaaaaaa", 16), 0);
  auto *next = static_cast<uint8_t *>(arena.allocate(8));
  EXPECT_EQ(static_cast<size_t>(next - b), round_up(16));  // b's space is untouched
  arena.deallocate(moved);
}

TEST(JsonArena, HeapBlocksReallocateOnTheHeap) {
  JsonArena<32> arena;
  void *a = arena.allocate(64);  // never fit
  EXPECT_FALSE(inside(arena, a));
  std::memset(a, 'h', 64);
  void *grown = arena.reallocate(a, 128);
  ASSERT_NE(grown, nullptr);
  EXPECT_EQ(std::memcmp(grown, "hhhhhhhhhhhhhhhh", 16), 0);
  arena.deallocate(grown);
}

TEST(JsonArena, FailedMoveKeepsTheBlockReserved) {
  NoMemory no_memory;
  JsonArena<64> arena(&no_memory);
  void *a = arena.allocate(32);
  std::memset(a, 'k', 32);
  EXPECT_EQ(arena.reallocate(a, 200), nullptr);
  EXPECT_EQ(std::memcmp(a, "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk", 32), 0);
  auto *b = static_cast<uint8_t *>(arena.allocate(16));  // must not hand out a's bytes again
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(static_cast<size_t>(b - static_cast<uint8_t *>(a)), round_up(32));
  EXPECT_EQ(arena.allocate(64), nullptr);  // nothing left and the fallback refuses
}

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
constexpr size_t ARENA = esphome::json::JSON_ARENA_SIZE;

// The documents the event stream sends must fit without touching the fallback, and their copied
// strings must land in the headroom above the pool
TEST(JsonArena, StateDocumentsFitWithoutTouchingTheFallback) {
  Counting counting;
  JsonArena<ARENA> arena(&counting);
  {
    // A switch state event: copied id, domain and name, bool value
    JsonBuilder builder(&arena);
    JsonObject root = builder.root();
    char id_buf[] = "switch/SSE Toggle";
    char domain_buf[] = "switch";
    char name_buf[] = "SSE Toggle";
    root["id"] = static_cast<const char *>(id_buf);
    root["domain"] = static_cast<const char *>(domain_buf);
    root["name"] = static_cast<const char *>(name_buf);
    root["icon"] = "";
    root["entity_category"] = 0;
    root["value"] = true;
    root["state"] = "ON";
    root["assumed_state"] = false;
    char out[256];
    EXPECT_LT(builder.serialize_to(out, sizeof(out)), sizeof(out));
  }
  EXPECT_EQ(counting.allocs, 0);
  EXPECT_GT(arena.used(), esphome::json::JSON_POOL_BYTES);

  Counting counting_select;
  JsonArena<ARENA> select_arena(&counting_select);
  {
    // A 40 option select detail document: copied id, domain, name and value, linked options
    JsonBuilder builder(&select_arena);
    JsonObject root = builder.root();
    char id_buf[] = "select/SSE Big Select";
    char domain_buf[] = "select";
    char name_buf[] = "SSE Big Select";
    char value_buf[] = "option number 17 padded to twenty";
    root["id"] = static_cast<const char *>(id_buf);
    root["domain"] = static_cast<const char *>(domain_buf);
    root["name"] = static_cast<const char *>(name_buf);
    root["icon"] = "";
    root["entity_category"] = 0;
    root["value"] = static_cast<const char *>(value_buf);
    root["state"] = static_cast<const char *>(value_buf);
    JsonArray options = root["option"].to<JsonArray>();
    char option_bufs[40][34];
    for (int i = 0; i < 40; i++) {
      snprintf(option_bufs[i], sizeof(option_bufs[i]), "option number %02d padded to twenty", i);
      options.add(JsonString(option_bufs[i], true));
    }
    char out[2048];
    EXPECT_LT(builder.serialize_to(out, sizeof(out)), sizeof(out));
  }
  EXPECT_EQ(counting_select.allocs, 0);
  EXPECT_GT(select_arena.used(), esphome::json::JSON_POOL_BYTES);

  // The same select with its 40 options copied, as the generator does before the strings are
  // linked, does not fit: the headroom is sized for linked options and the rest spills
  Counting counting_copied;
  JsonArena<ARENA> copied_arena(&counting_copied);
  {
    JsonBuilder builder(&copied_arena);
    JsonArray options = builder.root()["option"].to<JsonArray>();
    char option_bufs[40][34];
    for (int i = 0; i < 40; i++) {
      snprintf(option_bufs[i], sizeof(option_bufs[i]), "option number %02d padded to twenty", i);
      options.add(static_cast<const char *>(option_bufs[i]));
    }
    char out[2048];
    EXPECT_LT(builder.serialize_to(out, sizeof(out)), sizeof(out));
  }
  EXPECT_GT(counting_copied.allocs, 0);
  EXPECT_GT(copied_arena.used(), ARENA - 64);  // the arena filled up before the spill began
}

TEST(JsonArena, DocumentMatchesTheHeapAllocator) {
  // 700 integers need six pools, which also grows ArduinoJson's pool list past its preallocated four
  auto build = [](JsonBuilder &builder) {
    JsonArray arr = builder.root()["a"].to<JsonArray>();
    for (int i = 0; i < 700; i++) {
      arr.add(i);
    }
    JsonArray strings = builder.root()["s"].to<JsonArray>();
    char buf[32];
    for (int i = 0; i < 60; i++) {
      snprintf(buf, sizeof(buf), "string number %04d padded", i);
      strings.add(buf);
    }
  };
  JsonArena<ARENA> arena;
  JsonBuilder with_arena(&arena);
  build(with_arena);
  JsonBuilder with_heap;
  build(with_heap);
  std::string a = with_arena.serialize();
  std::string b = with_heap.serialize();
  EXPECT_GT(a.size(), 4000u);
  EXPECT_EQ(a, b);
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

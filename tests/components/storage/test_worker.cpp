#include <gtest/gtest.h>

#include "esphome/components/storage/storage_worker.h"

// Covers the parts of the worker that are pure logic: the transfer-job handle encoding and
// the tree walk's bounded path join. Everything else in storage_worker.cpp needs a storage
// device and a running scheduler, so it belongs in hardware testing rather than here.
//
// Both are worth pinning down for the same reason: they fail quietly. A handle that decodes
// to the wrong slot reports another transfer's progress, and a path join that truncates
// instead of refusing would copy into the wrong place.

namespace esphome::storage::testing {

// ---------------------------------------------------------------------------
// TransferJob encoding
// ---------------------------------------------------------------------------

TEST(TransferJobHandle, RoundTripsGenerationAndSlot) {
  const TransferJob job = make_transfer_job(7, 3);
  EXPECT_EQ(transfer_job_generation(job), 7u);
  EXPECT_EQ(transfer_job_slot(job), 3u);
}

TEST(TransferJobHandle, KeepsSlotAndGenerationApart) {
  // Two slots of one generation and one slot across two generations must all differ,
  // otherwise a stale handle could pass the generation check of a recycled slot.
  EXPECT_NE(make_transfer_job(1, 0), make_transfer_job(1, 1));
  EXPECT_NE(make_transfer_job(1, 0), make_transfer_job(2, 0));
}

TEST(TransferJobHandle, CoversEverySlotThePoolCanHold) {
  // max_pending is capped at 16, so the eight bits reserved for the slot are ample; check
  // the whole configurable range plus the byte boundary that bounds it.
  for (size_t slot : {size_t{0}, size_t{1}, size_t{15}, size_t{255}}) {
    const TransferJob job = make_transfer_job(1, slot);
    EXPECT_EQ(transfer_job_slot(job), slot);
    EXPECT_EQ(transfer_job_generation(job), 1u);
  }
}

TEST(TransferJobHandle, FirstGenerationIsNeverTheInvalidHandle) {
  // Generations start at 1 precisely so that slot 0's first handle is not 0, which is the
  // invalid handle every caller compares against.
  EXPECT_NE(make_transfer_job(1, 0), INVALID_TRANSFER_JOB);
}

TEST(TransferJobHandle, SurvivesALargeGeneration) {
  // Generations only ever increase, so a long-running node reaches large values; the shift
  // must not collide with the slot bits.
  const uint32_t big = 0x00FFFFFFu;
  const TransferJob job = make_transfer_job(big, 9);
  EXPECT_EQ(transfer_job_generation(job), big);
  EXPECT_EQ(transfer_job_slot(job), 9u);
}

// ---------------------------------------------------------------------------
// join_walk_path
// ---------------------------------------------------------------------------

TEST(JoinWalkPath, JoinsRootSubAndName) {
  char out[64];
  ASSERT_TRUE(join_walk_path(out, sizeof(out), "/sd/src", "a/b", "file.txt"));
  EXPECT_STREQ(out, "/sd/src/a/b/file.txt");
}

TEST(JoinWalkPath, SkipsTheSeparatorForAnEmptySub) {
  // "" is the walk's position at the top level, which is the common case for the first
  // entries of any tree -- a doubled separator here would reach the driver as a different
  // path than the one the walk means.
  char out[64];
  ASSERT_TRUE(join_walk_path(out, sizeof(out), "/sd/src", "", "file.txt"));
  EXPECT_STREQ(out, "/sd/src/file.txt");
}

TEST(JoinWalkPath, YieldsTheDirectoryWhenNoNameIsGiven) {
  char out[64];
  ASSERT_TRUE(join_walk_path(out, sizeof(out), "/sd/src", "a/b", nullptr));
  EXPECT_STREQ(out, "/sd/src/a/b");
}

TEST(JoinWalkPath, YieldsTheRootForAnEmptySubAndNoName) {
  char out[64];
  ASSERT_TRUE(join_walk_path(out, sizeof(out), "/sd/src", "", nullptr));
  EXPECT_STREQ(out, "/sd/src");
}

TEST(JoinWalkPath, RefusesWhatWouldNotFit) {
  // Refusing is the whole point: the walk turns a false into an error for that entry
  // instead of operating on a truncated path.
  char out[8];
  EXPECT_FALSE(join_walk_path(out, sizeof(out), "/sd/src", "a/b", "file.txt"));
}

TEST(JoinWalkPath, AcceptsAResultThatExactlyFits) {
  // "/a/b" is four characters plus the terminator -- the last size that must still succeed,
  // and the one an off-by-one in the bound check would reject.
  char out[5];
  ASSERT_TRUE(join_walk_path(out, sizeof(out), "/a", "", "b"));
  EXPECT_STREQ(out, "/a/b");
}

TEST(JoinWalkPath, RefusesOneCharacterPastTheBuffer) {
  char out[4];
  EXPECT_FALSE(join_walk_path(out, sizeof(out), "/a", "", "b"));
}

}  // namespace esphome::storage::testing

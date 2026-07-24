#include <gtest/gtest.h>

#include "esphome/components/storage/automation.h"

// Covers the extraction pipeline behind storage.file_read: extract_trim() and every
// ExtractStepType. These are pure string functions, which makes them the one part of the action
// layer that is fully testable without a device -- and the part where the interesting behaviour
// is in the edge cases rather than the happy path. The YAML tests only prove the steps compile.
//
// Two invariants are asserted throughout, because the action relies on both:
//   1. A structural failure returns false and leaves the buffer UNTOUCHED -- FileReadAction
//      aborts the pipeline on false and leaves the target global unwritten, so a half-applied
//      buffer would silently become the next step's input if this ever changed.
//   2. An EMPTY result is not a failure. A key that exists with no value, or a split element
//      between two adjacent separators, yields "" and returns true.

namespace esphome::storage::testing {

namespace {

// Applies one step to a copy, so a test can assert on the result and the return value without
// each case repeating the setup.
struct StepResult {
  bool ok;
  std::string buf;
};

// Not named `apply`: a std::string argument pulls namespace std into the ADL candidate set, where
// std::apply then wins and fails to instantiate (tuple_size<std::string>).
StepResult apply_step(const ExtractStep &step, const std::string &input) {
  std::string buf = input;
  const bool ok = apply_extract_step(step, buf);
  return {ok, buf};
}

ExtractStep line_step(int n) { return ExtractStep{ExtractStepType::LINE, "", "", n}; }
ExtractStep split_step(const std::string &sep, int index) {
  return ExtractStep{ExtractStepType::SPLIT, sep, "", index};
}
ExtractStep key_step(const std::string &key, const std::string &sep) {
  return ExtractStep{ExtractStepType::KEY, key, sep, 0};
}
ExtractStep trim_step() { return ExtractStep{ExtractStepType::TRIM, "", "", 0}; }

}  // namespace

// ---------------------------------------------------------------------------
// extract_trim
// ---------------------------------------------------------------------------

TEST(ExtractTrim, StripsEveryWhitespaceKindFromBothEnds) {
  EXPECT_EQ(extract_trim("  x \t\r\n"), "x");
  EXPECT_EQ(extract_trim("\r\n\tx"), "x");
  EXPECT_EQ(extract_trim("x"), "x");
}

TEST(ExtractTrim, KeepsWhitespaceInTheMiddle) { EXPECT_EQ(extract_trim("  a b  "), "a b"); }

TEST(ExtractTrim, YieldsEmptyForAnAllWhitespaceInput) {
  EXPECT_EQ(extract_trim(" \t\r\n"), "");
  EXPECT_EQ(extract_trim(""), "");
}

// ---------------------------------------------------------------------------
// LINE -- 1-based
// ---------------------------------------------------------------------------

TEST(ExtractLine, PicksTheRequestedLine) {
  EXPECT_EQ(apply_step(line_step(1), "a\nb\nc").buf, "a");
  EXPECT_EQ(apply_step(line_step(2), "a\nb\nc").buf, "b");
  EXPECT_EQ(apply_step(line_step(3), "a\nb\nc").buf, "c");
}

TEST(ExtractLine, StripsTheCarriageReturnOfACrlfFile) {
  const StepResult r = apply_step(line_step(1), "a\r\nb\r\n");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "a");
}

TEST(ExtractLine, CountsTheEmptyLineAfterATrailingNewline) {
  // "a\n" is two lines, the second empty -- not one line. Worth pinning down: nearly every text
  // file written by file_append ends in a newline, so an off-by-one here is the common case.
  const StepResult r = apply_step(line_step(2), "a\n");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "");
}

TEST(ExtractLine, FailsPastTheEndAndLeavesTheBufferUntouched) {
  const StepResult r = apply_step(line_step(4), "a\nb\nc");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "a\nb\nc");
}

TEST(ExtractLine, FailsForLineZeroBecauseCountingStartsAtOne) {
  const StepResult r = apply_step(line_step(0), "a\nb\nc");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "a\nb\nc");
}

// ---------------------------------------------------------------------------
// SPLIT -- 0-based
// ---------------------------------------------------------------------------

TEST(ExtractSplit, PicksTheRequestedElement) {
  EXPECT_EQ(apply_step(split_step(",", 0), "a,b,c").buf, "a");
  EXPECT_EQ(apply_step(split_step(",", 1), "a,b,c").buf, "b");
  EXPECT_EQ(apply_step(split_step(",", 2), "a,b,c").buf, "c");
}

TEST(ExtractSplit, HandlesAMultiCharacterSeparator) { EXPECT_EQ(apply_step(split_step("::", 1), "a::b::c").buf, "b"); }

TEST(ExtractSplit, YieldsAnEmptyElementBetweenAdjacentSeparators) {
  const StepResult r = apply_step(split_step(",", 1), "a,,c");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "");
}

TEST(ExtractSplit, YieldsTheWholeBufferForIndexZeroWhenTheSeparatorIsAbsent) {
  // A file that simply has no separator is not a structural failure for element 0 -- the whole
  // content IS the first element. Only asking for a later element fails.
  const StepResult first = apply_step(split_step(",", 0), "abc");
  EXPECT_TRUE(first.ok);
  EXPECT_EQ(first.buf, "abc");

  const StepResult second = apply_step(split_step(",", 1), "abc");
  EXPECT_FALSE(second.ok);
  EXPECT_EQ(second.buf, "abc");
}

TEST(ExtractSplit, FailsPastTheLastElementAndLeavesTheBufferUntouched) {
  const StepResult r = apply_step(split_step(",", 3), "a,b,c");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "a,b,c");
}

// ---------------------------------------------------------------------------
// KEY -- first line starting with "<key><separator>"
// ---------------------------------------------------------------------------

TEST(ExtractKey, YieldsTheRemainderOfTheMatchingLine) {
  const StepResult r = apply_step(key_step("name", "="), "x=1\nname=bob\ny=2");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "bob");
}

TEST(ExtractKey, TrimsTheLineBeforeMatchingAndAfterExtracting) {
  const StepResult r = apply_step(key_step("name", "="), "  name=bob  ");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "bob");
}

TEST(ExtractKey, TreatsCrlfLineEndingsLikeAnyOther) {
  const StepResult r = apply_step(key_step("name", "="), "a=1\r\nname=bob\r\n");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "bob");
}

TEST(ExtractKey, MatchesTheSeparatorExactlyWithoutSurroundingSpace) {
  // Documented gotcha rather than a wish: the needle is key + separator, so "name = bob" needs
  // `separator: " = "`. Changing this to tolerate spaces would silently change which line an
  // existing config matches, so it is pinned here.
  const StepResult strict = apply_step(key_step("name", "="), "name = bob");
  EXPECT_FALSE(strict.ok);
  EXPECT_EQ(strict.buf, "name = bob");

  const StepResult spaced = apply_step(key_step("name", " = "), "name = bob");
  EXPECT_TRUE(spaced.ok);
  EXPECT_EQ(spaced.buf, "bob");
}

TEST(ExtractKey, DoesNotMatchALongerKeyWithTheSamePrefix) {
  const StepResult r = apply_step(key_step("name", "="), "nameserver=x");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "nameserver=x");
}

TEST(ExtractKey, AcceptsAPresentButEmptyValue) {
  const StepResult r = apply_step(key_step("name", "="), "name=");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "");
}

TEST(ExtractKey, FailsWhenTheKeyIsAbsentAndLeavesTheBufferUntouched) {
  const StepResult r = apply_step(key_step("name", "="), "a=1\nb=2");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "a=1\nb=2");
}

// ---------------------------------------------------------------------------
// TRIM
// ---------------------------------------------------------------------------

TEST(ExtractStepTrim, AlwaysSucceedsEvenWhenNothingIsLeft) {
  const StepResult r = apply_step(trim_step(), " \t\r\n");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "");
}

// ---------------------------------------------------------------------------
// Composition -- what a real `extract:` list does
// ---------------------------------------------------------------------------

TEST(ExtractPipeline, AppliesStepsInOrder) {
  // The example from the docs: second line, second comma-separated field, trimmed.
  std::string buf = "header\nlabel , 21.5 , unit\ntrailer\n";
  ASSERT_TRUE(apply_extract_step(line_step(2), buf));
  ASSERT_TRUE(apply_extract_step(split_step(",", 1), buf));
  ASSERT_TRUE(apply_extract_step(trim_step(), buf));
  EXPECT_EQ(buf, "21.5");
}

TEST(ExtractPipeline, LeavesTheBufferUsableWhenAStepFails) {
  // FileReadAction aborts on the first false and never writes the global. The buffer being
  // untouched is what makes that abort safe to reason about.
  std::string buf = "only one line";
  EXPECT_TRUE(apply_extract_step(trim_step(), buf));
  EXPECT_FALSE(apply_extract_step(line_step(2), buf));
  EXPECT_EQ(buf, "only one line");
}

// ---------------------------------------------------------------------------
// REGEX -- only compiled when a config uses a `regex:` step (codegen sets the define)
// ---------------------------------------------------------------------------

#ifdef USE_STORAGE_REGEX_EXTRACT

TEST(ExtractRegex, GroupZeroIsTheWholeMatch) {
  ExtractStep step{ExtractStepType::REGEX, "([A-Za-z0-9_]+)", "", 0};
  const StepResult r = apply_step(step, "  name_1 = bob");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "name_1");
}

TEST(ExtractRegex, YieldsTheRequestedCaptureGroup) {
  ExtractStep step{ExtractStepType::REGEX, "v=([0-9]+)", "", 1};
  const StepResult r = apply_step(step, "x v=42 y");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "42");
}

TEST(ExtractRegex, FailsForAGroupThatDoesNotExist) {
  ExtractStep step{ExtractStepType::REGEX, "([A-Za-z0-9_]+)", "", 2};
  const StepResult r = apply_step(step, "name_1");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "name_1");
}

TEST(ExtractRegex, FailsWithoutAMatchAndLeavesTheBufferUntouched) {
  ExtractStep step{ExtractStepType::REGEX, "v=([0-9]+)", "", 1};
  const StepResult r = apply_step(step, "nothing here");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "nothing here");
}

#endif  // USE_STORAGE_REGEX_EXTRACT

// ---------------------------------------------------------------------------
// JSON -- only compiled when a config uses a `json:` step
// ---------------------------------------------------------------------------

#ifdef USE_STORAGE_JSON_EXTRACT

TEST(ExtractJson, ResolvesAPointerToAStringScalarUnquoted) {
  ExtractStep step{ExtractStepType::JSON, "a/b", "", 0};
  const StepResult r = apply_step(step, R"({"a":{"b":"x"}})");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "x");
}

TEST(ExtractJson, IndexesIntoAnArray) {
  ExtractStep step{ExtractStepType::JSON, "arr/1", "", 0};
  const StepResult r = apply_step(step, R"({"arr":["ten","twenty"]})");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.buf, "twenty");
}

TEST(ExtractJson, FailsForAMissingPathElementAndLeavesTheBufferUntouched) {
  ExtractStep step{ExtractStepType::JSON, "a/nope", "", 0};
  const std::string doc = R"({"a":{"b":"x"}})";
  const StepResult r = apply_step(step, doc);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, doc);
}

TEST(ExtractJson, FailsForADocumentThatDoesNotParse) {
  ExtractStep step{ExtractStepType::JSON, "a", "", 0};
  const StepResult r = apply_step(step, "not json at all");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.buf, "not json at all");
}

#endif  // USE_STORAGE_JSON_EXTRACT

}  // namespace esphome::storage::testing

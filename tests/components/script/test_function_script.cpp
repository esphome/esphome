#include <gtest/gtest.h>

#include "esphome/components/script/script.h"

namespace esphome::script::testing {

// Each body is the function of its script, as code generation emits it.
static void single_body(const int &running);
using TestSingle = FunctionScript<SingleMode<FunctionBody<>>, single_body>;
static TestSingle *single_script;
static int single_runs;
static bool single_running_inside;
static void single_body(const int &running) {
  single_runs++;
  single_running_inside = single_script->is_running();
  // Only once, so a script that is not running here fails the test instead of recursing
  if (single_runs == 1)
    single_script->execute();
}

TEST(FunctionScriptTest, SingleIsRunningOnlyInsideItsBody) {
  TestSingle script;
  single_script = &script;
  single_runs = 0;
  script.execute();
  EXPECT_EQ(single_runs, 1);
  EXPECT_TRUE(single_running_inside);
  EXPECT_FALSE(script.is_running());
}

static void stop_body(const int &running);
using TestStop = FunctionScript<SingleMode<FunctionBody<>>, stop_body>;
static TestStop *stop_script;
static int stop_running_after;
static void stop_body(const int &running) {
  stop_script->stop();
  stop_running_after = running;
}

TEST(FunctionScriptTest, StopInsideClearsTheRunCount) {
  TestStop script;
  stop_script = &script;
  stop_running_after = -1;
  script.execute();
  EXPECT_EQ(stop_running_after, 0);
  EXPECT_FALSE(script.is_running());
}

static void restart_body(const int &running);
using TestRestart = FunctionScript<RestartMode<FunctionBody<>>, restart_body>;
static TestRestart *restart_script;
static int restart_starts;
static int restart_ends;
static void restart_body(const int &running) {
  if (restart_starts++ == 0)
    restart_script->execute();
  if (running)
    restart_ends++;
}

TEST(FunctionScriptTest, RestartFromInsideStopsTheOuterRun) {
  TestRestart script;
  restart_script = &script;
  restart_starts = restart_ends = 0;
  script.execute();
  EXPECT_EQ(restart_starts, 2);
  EXPECT_EQ(restart_ends, 1);
  EXPECT_FALSE(script.is_running());
}

static void parallel_body(const int &running, const int &n);
using TestParallel = FunctionScript<ParallelMode<FunctionBody<int>, int>, parallel_body, int>;
static TestParallel *parallel_script;
static int parallel_runs;
static void parallel_body(const int &running, const int &n) {
  parallel_runs++;
  if (n < 5)
    parallel_script->execute(n + 1);
}

TEST(FunctionScriptTest, ParallelCountsNestedRuns) {
  TestParallel script;
  script.set_max_runs(2);
  parallel_script = &script;
  parallel_runs = 0;
  script.execute(1);
  EXPECT_EQ(parallel_runs, 2);
  EXPECT_FALSE(script.is_running());
}

static void queued_body(const int &running, const int &n);
using TestQueued = FunctionScript<QueueingMode<FunctionBody<int>, int>, queued_body, int>;
static TestQueued *queued_script;
static int queued_last;
static int queued_runs;
static void queued_body(const int &running, const int &n) {
  queued_runs++;
  queued_last = n;
  if (n == 1) {
    queued_script->execute(2);
    queued_script->execute(3);
  }
}

TEST(FunctionScriptTest, QueuedRunsTheQueuedRunAfterTheBody) {
  TestQueued script;
  script.set_max_runs(2);
  queued_script = &script;
  queued_runs = 0;
  script.execute(1);
  EXPECT_EQ(queued_runs, 2);
  EXPECT_EQ(queued_last, 2);
  EXPECT_FALSE(script.is_running());
}

}  // namespace esphome::script::testing

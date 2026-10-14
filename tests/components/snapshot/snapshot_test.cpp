#ifdef USE_HOST
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <thread>
#include <vector>
#include "esphome/components/snapshot/snapshot.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::snapshot::testing {
namespace fs = std::filesystem;

/// A display of a fixed size that can be told to fail a capture.
class FakeSnapshot : public Snapshot {
 public:
  /// The capture with this number (counting from 1) fails. Zero means none do.
  int fail_on{0};
  int captures{0};

 protected:
  int snapshot_width() override { return 8; }
  int snapshot_height() override { return 4; }
  bool capture_bgr(uint8_t *dest, size_t row_stride) override {
    this->captures++;
    if (this->captures == this->fail_on)
      return false;
    for (int y = 0; y != 4; y++) {
      for (int x = 0; x != 8 * 3; x++)
        dest[y * row_stride + x] = static_cast<uint8_t>(x * 7 + y * 31 + this->captures);
    }
    return true;
  }
};

class SnapshotAnimationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->dir_ = fs::temp_directory_path() / "esphome_snapshot_test";
    fs::remove_all(this->dir_);
    fs::create_directories(this->dir_);
    setenv("ESPHOME_SNAPSHOT_DIR", this->dir_.c_str(), 1);
    // The test main does not construct App as generated code does, and recording needs its scheduler.
    static const bool app_constructed = (new (&App) Application(), true);
    (void) app_constructed;
    App.pre_setup("test_snapshot", 10, "", 0);
  }
  void TearDown() override {
    unsetenv("ESPHOME_SNAPSHOT_DIR");
    fs::remove_all(this->dir_);
  }

  /// Names of the files in the snapshot directory, in order.
  std::vector<std::string> files() const {
    std::vector<std::string> names;
    for (const auto &entry : fs::directory_iterator(this->dir_))
      names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
  }

  /// Let the scheduler run for long enough that a fast recording has finished.
  static void run_scheduler() {
    for (int i = 0; i != 50; i++) {
      App.scheduler.call(millis());
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  fs::path dir_;
};

TEST_F(SnapshotAnimationTest, FailedFirstCaptureLeavesNoFile) {
  FakeSnapshot display;
  display.fail_on = 1;
  EXPECT_FALSE(display.take_animation("first.gif", 3, 50.0f));
  EXPECT_TRUE(this->files().empty());
}

TEST_F(SnapshotAnimationTest, FailedCaptureMidRecordingRemovesPartialFile) {
  FakeSnapshot display;
  display.fail_on = 2;
  ASSERT_TRUE(display.take_animation("partial.gif", 3, 50.0f));
  // The first frame is already in the file.
  EXPECT_EQ(this->files(), std::vector<std::string>{"partial.gif"});

  run_scheduler();
  EXPECT_TRUE(this->files().empty());
  // The failure ends the recording, so the display can record again.
  display.fail_on = 0;
  EXPECT_TRUE(display.take_animation("again.gif", 1, 50.0f));
  EXPECT_EQ(this->files(), std::vector<std::string>{"again.gif"});
}

TEST_F(SnapshotAnimationTest, SecondRecordingIsRefusedUntilTheFirstEnds) {
  FakeSnapshot display;
  ASSERT_TRUE(display.take_animation("one.gif", 3, 50.0f));
  EXPECT_FALSE(display.take_animation("two.gif", 3, 50.0f));
  EXPECT_EQ(this->files(), std::vector<std::string>{"one.gif"});

  run_scheduler();
  std::ifstream in(this->dir_ / "one.gif", std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  ASSERT_FALSE(data.empty());
  EXPECT_EQ(data.back(), ';');  // the GIF trailer: the recording finished

  EXPECT_TRUE(display.take_animation("two.gif", 1, 50.0f));
  EXPECT_EQ(this->files(), (std::vector<std::string>{"one.gif", "two.gif"}));
}

TEST_F(SnapshotAnimationTest, RejectsNoFramesAndNoFrameRate) {
  FakeSnapshot display;
  EXPECT_FALSE(display.take_animation("none.gif", 0, 10.0f));
  EXPECT_FALSE(display.take_animation("none.gif", 3, 0.0f));
  EXPECT_TRUE(this->files().empty());
}

}  // namespace esphome::snapshot::testing
#endif

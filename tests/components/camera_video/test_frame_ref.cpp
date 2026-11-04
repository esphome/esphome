#include <utility>

#include "gtest/gtest.h"
#include "esphome/components/camera_video/frame_ref.h"

namespace esphome::camera_video {
namespace {

struct TestFrame : RefCountedFrame {
  explicit TestFrame(int *releases) : releases(releases) {}
  void on_last_reference() override { ++*this->releases; }
  int *releases;
};

TEST(CameraVideoFrameRef, CopyMoveAndReset) {
  int releases = 0;
  TestFrame frame(&releases);
  {
    FrameRef<TestFrame> first(&frame);
    EXPECT_TRUE(frame.has_references());
    {
      auto second = first;
      auto third = std::move(second);
      EXPECT_TRUE(third);
      first.reset();
      EXPECT_EQ(releases, 0);
    }
    EXPECT_EQ(releases, 1);
    EXPECT_FALSE(frame.has_references());
  }
  {
    FrameRef<TestFrame> ref(&frame);
    EXPECT_EQ(releases, 1);
    ref = nullptr;
    EXPECT_EQ(releases, 2);
    EXPECT_FALSE(frame.has_references());
  }
}

}  // namespace
}  // namespace esphome::camera_video

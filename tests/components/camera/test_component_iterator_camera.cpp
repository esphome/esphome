#include <gtest/gtest.h>

#include "esphome/core/component_iterator.h"

#ifdef USE_CAMERA
#include "esphome/components/camera/camera.h"

namespace esphome::testing {

class StubCamera : public camera::Camera {
 public:
  void add_listener(camera::CameraListener *listener) override {}
  camera::CameraImageReader *create_image_reader() override { return nullptr; }
  void request_image(camera::CameraRequester requester) override {}
  void start_stream(camera::CameraRequester requester) override {}
  void stop_stream(camera::CameraRequester requester) override {}
};

// Iterator that accepts everything except the camera, which can refuse a
// configurable number of times. The CAMERA state is a singleton path
// distinct from process_platform_item_; this pins the same contract:
// a refused camera is re-offered, never skipped.
class CameraRefusingIterator : public ComponentIterator {
 public:
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) \
  bool on_##singular(type *obj) override { return true; }
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)

  bool on_camera(camera::Camera *obj) override {
    this->camera_calls++;
    if (this->camera_refusals > 0) {
      this->camera_refusals--;
      return false;
    }
    return true;
  }

  int camera_calls{0};
  int camera_refusals{0};
};

// Far above the fixed number of iterator states
static constexpr size_t BIG_BUDGET = 1000;

class ComponentIteratorCameraTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Constructing a Camera installs the process-wide singleton
    static StubCamera stub_camera;
    ASSERT_EQ(camera::Camera::instance(), &stub_camera);
  }
};

TEST_F(ComponentIteratorCameraTest, RefusedCameraIsReofferedNotSkipped) {
  CameraRefusingIterator it;
  it.camera_refusals = 2;
  it.begin();
  // Runs until the camera refuses, which stops the pass
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.camera_calls, 1);
  EXPECT_FALSE(it.completed());
  // The camera is re-offered once per call, not skipped
  it.try_advance(BIG_BUDGET);
  EXPECT_EQ(it.camera_calls, 2);
  EXPECT_FALSE(it.completed());
  // Once accepted, the iteration completes
  it.try_advance(BIG_BUDGET);
  EXPECT_TRUE(it.completed());
  EXPECT_EQ(it.camera_calls, 3);
}

}  // namespace esphome::testing
#endif  // USE_CAMERA

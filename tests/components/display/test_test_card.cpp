#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <new>
#include <vector>

#include "esphome/components/display/display.h"
#include "esphome/core/application.h"

namespace esphome::display::testing {

// Records the colour of every pixel written, so a drawn test card shows up as non-black pixels.
class TestCardDisplay : public Display {
 public:
  void draw_pixel_at(int x, int y, Color color) override { this->colors_.push_back(color.raw_32); }
  void update() override { this->do_update_(); }
  DisplayType get_display_type() override { return DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return 64; }
  int get_height_internal() override { return 64; }

  bool drew_test_card() const {
    return std::any_of(this->colors_.begin(), this->colors_.end(), [](uint32_t c) { return c != 0; });
  }
  void reset() { this->colors_.clear(); }

 protected:
  std::vector<uint32_t> colors_;
};

static int writer_calls = 0;
static void count_writer(Display &) { writer_calls++; }

class TestCard : public ::testing::Test {
 protected:
  void SetUp() override {
    // test_card() stops the poller, which needs the scheduler; the test main does not construct App.
    static const bool app_constructed = (new (&App) Application(), true);
    (void) app_constructed;
    writer_calls = 0;
  }
};

TEST_F(TestCard, DrawnOnEveryUpdateInsteadOfLambda) {
  TestCardDisplay display;
  display.set_writer(count_writer);
  display.show_test_card();
  for (int i = 0; i != 3; i++) {
    display.reset();
    display.update();
    EXPECT_TRUE(display.drew_test_card()) << "update " << i;
  }
  EXPECT_EQ(writer_calls, 0);
}

TEST_F(TestCard, DrawnOnEveryUpdateInsteadOfPages) {
  TestCardDisplay display;
  DisplayPage page(count_writer);
  display.set_pages({&page});
  display.show_test_card();
  for (int i = 0; i != 3; i++) {
    display.reset();
    display.update();
    EXPECT_TRUE(display.drew_test_card()) << "update " << i;
  }
  EXPECT_EQ(writer_calls, 0);
}

TEST_F(TestCard, LambdaRunsWithoutTestCard) {
  TestCardDisplay display;
  display.set_writer(count_writer);
  display.update();
  display.update();
  EXPECT_FALSE(display.drew_test_card());
  EXPECT_EQ(writer_calls, 2);
}

}  // namespace esphome::display::testing

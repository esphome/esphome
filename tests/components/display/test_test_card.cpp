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

  bool drew_test_card() const {
    return std::any_of(this->colors_.begin(), this->colors_.end(), [](uint32_t c) { return c != 0; });
  }
  void reset() { this->colors_.clear(); }

 protected:
  int get_width_internal() override { return 64; }
  int get_height_internal() override { return 64; }
  std::vector<uint32_t> colors_;
};

namespace {
class TestCard : public ::testing::Test {
 protected:
  int writer_calls_{0};
  display_writer_t count_writer_() {
    return [this](Display &) { this->writer_calls_++; };
  }
};
}  // namespace

TEST_F(TestCard, DrawnOnEveryUpdateInsteadOfLambda) {
  TestCardDisplay display;
  display.set_writer(this->count_writer_());
  display.show_test_card();
  for (int i = 0; i != 3; i++) {
    display.reset();
    display.update();
    EXPECT_TRUE(display.drew_test_card()) << "update " << i;
  }
  EXPECT_EQ(this->writer_calls_, 0);
}

TEST_F(TestCard, DrawnOnEveryUpdateInsteadOfPages) {
  TestCardDisplay display;
  DisplayPage page(this->count_writer_());
  display.set_pages({&page});
  display.show_test_card();
  for (int i = 0; i != 3; i++) {
    display.reset();
    display.update();
    EXPECT_TRUE(display.drew_test_card()) << "update " << i;
  }
  EXPECT_EQ(this->writer_calls_, 0);
}

TEST_F(TestCard, LambdaRunsWithoutTestCard) {
  TestCardDisplay display;
  display.set_writer(this->count_writer_());
  display.update();
  display.update();
  EXPECT_FALSE(display.drew_test_card());
  EXPECT_EQ(this->writer_calls_, 2);
}

}  // namespace esphome::display::testing

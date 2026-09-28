#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/matrix_keypad/matrix_keypad.h"
#include "esphome/core/application.h"

namespace esphome::matrix_keypad::testing {

class RowPin : public GPIOPin {
 public:
  void setup() override {}
  void pin_mode(gpio::Flags flags) override { this->flags_ = flags; }
  gpio::Flags get_flags() const override { return this->flags_; }
  bool digital_read() override { return this->level_; }
  void digital_write(bool value) override { this->level_ = value; }
  bool scanning() const { return this->flags_ == gpio::FLAG_OUTPUT && !this->level_; }

 protected:
  gpio::Flags flags_{gpio::FLAG_OUTPUT};
  bool level_{true};
};

class ColumnPin : public GPIOPin {
 public:
  ColumnPin(std::array<RowPin, 2> &rows, std::array<bool, 4> &buttons, int column)
      : rows_(rows), buttons_(buttons), column_(column) {}

  void setup() override {}
  void pin_mode(gpio::Flags flags) override { this->flags_ = flags; }
  gpio::Flags get_flags() const override { return this->flags_; }
  bool digital_read() override {
    for (int row = 0; row < 2; row++) {
      if (this->rows_[row].scanning() && this->buttons_[row * 2 + this->column_])
        return false;
    }
    return true;
  }
  void digital_write(bool) override {}

 protected:
  std::array<RowPin, 2> &rows_;
  std::array<bool, 4> &buttons_;
  int column_;
  gpio::Flags flags_{gpio::FLAG_INPUT};
};

class RecordingListener : public MatrixKeypadListener {
 public:
  void button_pressed(int row, int col) override { this->pressed_buttons.push_back(row * 2 + col); }
  void button_released(int row, int col) override { this->released_buttons.push_back(row * 2 + col); }
  void key_pressed(uint8_t key) override { this->pressed_keys.push_back(key); }
  void key_released(uint8_t key) override { this->released_keys.push_back(key); }

  std::vector<int> pressed_buttons;
  std::vector<int> released_buttons;
  std::vector<uint8_t> pressed_keys;
  std::vector<uint8_t> released_keys;
};

class MatrixKeypadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->keypad_.set_rows({&this->rows_[0], &this->rows_[1]});
    this->keypad_.set_columns({&this->column_0_, &this->column_1_});
    this->keypad_.set_keys("ABCD");
    this->keypad_.set_debounce_time(20);
    this->keypad_.register_listener(&this->listener_);
    this->keypad_.add_on_key_callback([this](uint8_t key) { this->provider_keys_.push_back(key); });
  }

  void scan(uint32_t now) {
    LoopBlockingGuard guard(&this->keypad_, nullptr, now);
    this->keypad_.loop();
  }

  std::array<RowPin, 2> rows_{};
  std::array<bool, 4> buttons_{};
  ColumnPin column_0_{rows_, buttons_, 0};
  ColumnPin column_1_{rows_, buttons_, 1};
  MatrixKeypad keypad_;
  RecordingListener listener_;
  std::vector<uint8_t> provider_keys_;
};

TEST_F(MatrixKeypadTest, DiodesReportSimultaneousPressesAndIndependentReleases) {
  this->keypad_.set_has_diodes(true);
  this->keypad_.setup();
  this->buttons_[0] = true;
  this->buttons_[3] = true;
  this->scan(100);
  this->scan(119);
  EXPECT_TRUE(this->listener_.pressed_buttons.empty());
  this->scan(120);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0, 3}));
  EXPECT_EQ(this->listener_.pressed_keys, (std::vector<uint8_t>{'A', 'D'}));
  EXPECT_EQ(this->provider_keys_, (std::vector<uint8_t>{'A', 'D'}));

  this->buttons_[0] = false;
  this->scan(121);
  EXPECT_EQ(this->listener_.released_buttons, (std::vector<int>{0}));
  EXPECT_EQ(this->listener_.released_keys, (std::vector<uint8_t>{'A'}));
  this->scan(150);
  EXPECT_EQ(this->listener_.pressed_buttons.size(), 2);
  this->buttons_[3] = false;
  this->scan(151);
  EXPECT_EQ(this->listener_.released_buttons, (std::vector<int>{0, 3}));
  EXPECT_EQ(this->listener_.released_keys, (std::vector<uint8_t>{'A', 'D'}));
}

TEST_F(MatrixKeypadTest, DiodesDebounceEachPressIndependently) {
  this->keypad_.set_has_diodes(true);
  this->keypad_.setup();
  this->buttons_[0] = true;
  this->scan(100);
  this->buttons_[3] = true;
  this->scan(110);
  this->scan(120);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0}));

  this->buttons_[0] = false;
  this->scan(125);
  this->buttons_[0] = true;
  this->scan(126);
  this->scan(130);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0, 3}));
  this->scan(145);
  EXPECT_EQ(this->listener_.pressed_buttons.size(), 2);
  this->scan(146);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0, 3, 0}));
}

TEST_F(MatrixKeypadTest, DiodesIgnoreShortPressWithoutAffectingAnotherKey) {
  this->keypad_.set_has_diodes(true);
  this->keypad_.setup();
  this->buttons_[0] = true;
  this->buttons_[3] = true;
  this->scan(100);
  this->buttons_[0] = false;
  this->scan(110);
  this->scan(120);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{3}));
  EXPECT_TRUE(this->listener_.released_buttons.empty());
  this->buttons_[0] = true;
  this->scan(121);
  this->scan(140);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{3}));
  this->scan(141);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{3, 0}));
}

TEST_F(MatrixKeypadTest, DiodesReportButtonsWithoutKeyCodes) {
  this->keypad_.set_keys("");
  this->keypad_.set_has_diodes(true);
  this->keypad_.setup();
  this->buttons_[0] = true;
  this->buttons_[1] = true;
  this->buttons_[3] = true;
  this->scan(100);
  this->scan(120);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0, 1, 3}));
  EXPECT_TRUE(this->listener_.pressed_keys.empty());
  EXPECT_TRUE(this->provider_keys_.empty());
  this->buttons_[1] = false;
  this->scan(121);
  EXPECT_EQ(this->listener_.released_buttons, (std::vector<int>{1}));
  EXPECT_TRUE(this->listener_.released_keys.empty());
}

TEST_F(MatrixKeypadTest, WithoutDiodesAmbiguousScanLeavesPreviousKeyPressed) {
  this->keypad_.setup();
  this->buttons_[0] = true;
  this->scan(100);
  this->scan(120);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0}));

  this->buttons_[3] = true;
  this->scan(121);
  this->scan(150);
  EXPECT_TRUE(this->listener_.released_buttons.empty());
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0}));
  this->buttons_[0] = false;
  this->scan(151);
  EXPECT_EQ(this->listener_.released_buttons, (std::vector<int>{0}));
  this->scan(171);
  EXPECT_EQ(this->listener_.pressed_buttons, (std::vector<int>{0, 3}));
}

}  // namespace esphome::matrix_keypad::testing

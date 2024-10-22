#pragma once

#include <utility>
#include <iomanip>
#include <sstream>

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/font/font.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace graphical_layout {

class TextRunPanel;

struct CanWrapAtCharacterArguments {
  CanWrapAtCharacterArguments(const TextRunPanel *panel, int offset, std::string string, char character) {
    this->panel = panel;
    this->offset = offset;
    this->string = std::move(string);
    this->character = character;
  }

  const TextRunPanel *panel;
  int offset;
  std::string string;
  char character;
};

class TextRunBase {
 public:
  TextRunBase(display::BaseFont *font) { this->font_ = font; }

  void set_foreground_color(Color foreground_color) { this->foreground_color_ = foreground_color; }
  void set_background_color(Color background_color) { this->background_color_ = background_color; }
  virtual std::string get_text() = 0;

  display::BaseFont *font_{nullptr};
  Color foreground_color_{COLOR_ON};
  Color background_color_{COLOR_OFF};
};

class FormattableTextRun {
 public:
  template<typename V> void set_text_formatter(V text_formatter) { this->text_formatter_ = text_formatter; };

  std::string format_text(std::string string) {
    if (this->text_formatter_.has_value()) {
      return this->text_formatter_.value(string);
    }

    return string;
  }

 protected:
  TemplatableValue<std::string, const std::string> text_formatter_{};
};

class TextRun : public TextRunBase, public FormattableTextRun {
 public:
  TextRun(TemplatableValue<std::string> text, display::BaseFont *font) : TextRunBase(font) {
    this->text_ = std::move(text);
  }

  std::string get_text() override { return this->format_text(this->text_.value()); }

 protected:
  TemplatableValue<std::string> text_{};
};

class ParagraphBreakTextRun : public TextRunBase {
 public:
  ParagraphBreakTextRun(size_t breaks, display::BaseFont *font) : TextRunBase(font) {
    this->text_.append(breaks, '\n');
  }

  std::string get_text() override { return this->text_; }

 protected:
  std::string text_{""};
};

class SensorTextRun : public TextRunBase, public FormattableTextRun {
 public:
  SensorTextRun(sensor::Sensor *sensor, display::BaseFont *font) : TextRunBase(font) { this->sensor_ = sensor; }

  std::string get_text() override {
    std::stringstream stream;
    stream << std::fixed << std::setprecision(this->sensor_->get_accuracy_decimals()) << this->sensor_->get_state();
    return this->format_text(stream.str());
  }

 protected:
  sensor::Sensor *sensor_{nullptr};
};

class TextSensorTextRun : public TextRunBase, public FormattableTextRun {
 public:
  TextSensorTextRun(text_sensor::TextSensor *text_sensor, display::BaseFont *font) : TextRunBase(font) {
    this->text_sensor_ = text_sensor;
  }

  std::string get_text() override { return this->format_text(this->text_sensor_->get_state()); }

 protected:
  text_sensor::TextSensor *text_sensor_{nullptr};
};

class TimeTextRun : public TextRunBase, public FormattableTextRun {
 public:
  TimeTextRun(time::RealTimeClock *time, TemplatableValue<std::string> time_format, bool use_utc_time,
              display::BaseFont *font)
      : TextRunBase(font) {
    this->time_ = time;
    this->time_format_ = std::move(time_format);
    this->use_utc_time_ = use_utc_time;
  }

  std::string get_text() override {
    ESPTime time;
    if (this->use_utc_time_) {
      time = this->time_->utcnow();
    } else {
      time = this->time_->now();
    }

    return this->format_text(time.strftime(this->time_format_.value()));
  }

 protected:
  time::RealTimeClock *time_{nullptr};
  TemplatableValue<std::string> time_format_{""};
  bool use_utc_time_{false};
};

struct RunProperties {
  bool is_white_space{false};

  /**
   * If false the run will be skipped
   */
  bool is_printable{false};

  /*
   * A run with this marked as true will not be printed when it appears at the start of a line. Useful for whitespace
   */
  bool suppress_at_start_of_line{false};

  /*
   * A run with this marked as true will not be printed when it appears at the end of a line. Useful for white space
   */
  bool suppress_at_end_of_line{false};

  /**
   * If true can allow the the line to end
   */
  bool can_wrap{false};

  /**
   * If true will force a new line
   */
  bool causes_new_line{false};

  bool is_equivalent(const RunProperties &compare) {
    return this->is_white_space == compare.is_white_space && this->is_printable == compare.is_printable &&
           this->suppress_at_start_of_line == compare.suppress_at_start_of_line &&
           this->suppress_at_end_of_line == compare.suppress_at_end_of_line && this->can_wrap == compare.can_wrap &&
           this->causes_new_line == compare.causes_new_line;
  }
};

class CalculatedTextRun {
 public:
  CalculatedTextRun(std::shared_ptr<TextRunBase> run, RunProperties run_properties) {
    this->run = std::move(run);
    this->run_properties = run_properties;
  }

  void calculate_bounds() {
    int x1;
    int width;
    int height;
    int baseline;

    this->run->font_->measure(this->text.c_str(), &width, &x1, &baseline, &height);

    this->baseline = baseline;
    this->bounds = display::Rect(0, 0, width, height);
  }

  std::string text{};
  display::Rect bounds{};
  std::shared_ptr<TextRunBase> run{};
  int16_t baseline{0};
  RunProperties run_properties{};
};

class LineInfo {
 public:
  LineInfo(int line_number) { this->line_number = line_number; }

  void add_run(const std::shared_ptr<CalculatedTextRun> &run) {
    this->total_width += run->bounds.w;
    this->max_height = std::max(this->max_height, run->bounds.h);
    this->max_baseline = std::max(this->max_baseline, run->baseline);
    this->runs.push_back(run);
  }

  void pop_last_run() { this->runs.pop_back(); }

  void recalculate_line_measurements() {
    int16_t total_width = 0;
    int16_t max_height = 0;
    int16_t max_baseline = 0;

    for (const auto &run : this->runs) {
      total_width += run->bounds.w;
      max_height = std::max(max_height, run->bounds.h);
      max_baseline = std::max(max_baseline, run->baseline);
    }

    this->total_width = total_width;
    this->max_height = max_height;
    this->max_baseline = max_baseline;
  }

  std::vector<std::shared_ptr<CalculatedTextRun>> runs;
  int16_t line_number{0};
  int16_t max_height{0};
  int16_t total_width{0};
  int16_t max_baseline{0};
};

struct CalculatedLayout {
  std::vector<std::shared_ptr<LineInfo>> lines;
  display::Rect bounds;
};

struct CharacterProperties : RunProperties {
  char character{'\0'};
  optional<std::string> replace_with{};
};

/** The TextRunPanel is a UI item that renders a multiple "runs" of text of independent styling to a display */
class TextRunPanel : public LayoutItem {
 public:
  display::Rect measure_item_internal(display::Display *display) override;
  void render_internal(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;
  void setup_complete() override;

  bool default_can_wrap_at_character(const CanWrapAtCharacterArguments &args);

  template<typename V> void set_can_wrap_at(V can_wrap_at_character) {
    this->can_wrap_at_character_ = can_wrap_at_character;
  };

  void add_text_run(const std::shared_ptr<TextRunBase> &text_run) { this->text_runs_.push_back(text_run); };
  void set_text_align(display::TextAlign text_align) { this->text_align_ = text_align; };
  void set_min_width(int min_width) { this->min_width_ = min_width; };
  void set_max_width(int max_width) { this->max_width_ = max_width; };
  void set_draw_partial_lines(bool draw_partial_lines) { this->draw_partial_lines_ = draw_partial_lines; };
  void set_debug_outline_runs(bool debug_outline_runs) { this->debug_outline_runs_ = debug_outline_runs; };

 protected:
  CalculatedLayout determine_layout_(display::Display *display, display::Rect bounds, bool grow_beyond_bounds_height);
  std::vector<std::shared_ptr<CalculatedTextRun>> split_runs_into_words_();
  std::vector<std::shared_ptr<LineInfo>> fit_words_to_bounds_(
      const std::vector<std::shared_ptr<CalculatedTextRun>> &runs, display::Rect bounds,
      bool grow_beyond_bounds_height);
  void apply_alignment_to_lines_(std::vector<std::shared_ptr<LineInfo>> &lines, display::TextAlign alignment);
  CharacterProperties get_character_properties_(char character);

  std::vector<std::shared_ptr<TextRunBase>> text_runs_;
  display::TextAlign text_align_{display::TextAlign::TOP_LEFT};
  int min_width_{0};
  int max_width_{0};
  TemplatableValue<bool, const CanWrapAtCharacterArguments &> can_wrap_at_character_{};
  bool draw_partial_lines_{false};
  bool debug_outline_runs_{false};
};

}  // namespace graphical_layout
}  // namespace esphome

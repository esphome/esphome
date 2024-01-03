#pragma once

#include <utility>
#include <iomanip>
#include <sstream>

#include "esphome/components/graphical_layout/graphical_layout.h"
#include "esphome/components/font/font.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

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

  std::string get_text() override { return this->format_text(text_.value()); }

 protected:
  TemplatableValue<std::string> text_{};
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

class CalculatedTextRun {
 public:
  CalculatedTextRun(TextRunBase *run, std::string text, display::Rect bounds, int16_t baseline, int16_t line_number) {
    this->run = run;
    this->text_ = std::move(text);
    this->bounds = bounds;
    this->baseline = baseline;
    this->line_number_ = line_number;
  }

  std::string text_;
  display::Rect bounds;
  TextRunBase *run;
  int16_t line_number_;
  int16_t baseline;
};

struct CalculatedLayout {
  std::vector<std::shared_ptr<CalculatedTextRun>> runs;
  display::Rect bounds;
  int line_count;
};

/** The TextRunPanel is a UI item that renders a multiple "runs" of text of independent styling to a display */
class TextRunPanel : public LayoutItem {
 public:
  display::Rect measure_item_internal(display::Display *display) override;
  void render_internal(display::Display *display, display::Rect bounds) override;
  void dump_config(int indent_depth, int additional_level_depth) override;
  void setup_complete() override;

  bool default_can_wrap_at_character(const CanWrapAtCharacterArguments &args);

  CalculatedLayout determine_layout(display::Display *display, display::Rect bounds, bool apply_alignment);
  void apply_alignment_to_layout(CalculatedLayout *layout);

  template<typename V> void set_can_wrap_at(V can_wrap_at_character) {
    this->can_wrap_at_character_ = can_wrap_at_character;
  };

  void add_text_run(TextRunBase *text_run) { this->text_runs_.push_back(text_run); };
  void set_text_align(display::TextAlign text_align) { this->text_align_ = text_align; };
  void set_min_width(int min_width) { this->min_width_ = min_width; };
  void set_max_width(int max_width) { this->max_width_ = max_width; };
  void set_debug_outline_runs(bool debug_outline_runs) { this->debug_outline_runs_ = debug_outline_runs; };

 protected:
  std::vector<TextRunBase *> text_runs_;
  display::TextAlign text_align_{display::TextAlign::TOP_LEFT};
  int min_width_{0};
  int max_width_{0};
  TemplatableValue<bool, const CanWrapAtCharacterArguments &> can_wrap_at_character_{};
  bool debug_outline_runs_{false};
};

}  // namespace graphical_layout
}  // namespace esphome

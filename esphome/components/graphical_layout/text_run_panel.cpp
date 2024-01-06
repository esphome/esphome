#include "text_run_panel.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "textrunpanel";
static const int TEXT_ALIGN_X_MASK = (int) display::TextAlign::RIGHT | (int) display::TextAlign::CENTER_HORIZONTAL;
static const int TEXT_ALIGN_Y_MASK =
    (int) display::TextAlign::BOTTOM | (int) display::TextAlign::BASELINE | (int) display::TextAlign::CENTER_VERTICAL;

void TextRunPanel::dump_config(int indent_depth, int additional_level_depth) {
  this->dump_config_base_properties(TAG, indent_depth);
  ESP_LOGCONFIG(TAG, "%*sMin Width: %i", indent_depth, "", this->min_width_);
  ESP_LOGCONFIG(TAG, "%*sMax Width: %i", indent_depth, "", this->max_width_);
  ESP_LOGCONFIG(TAG, "%*sText Align: %s", indent_depth, "",
                LOG_STR_ARG(display::text_align_to_string(this->text_align_)));
  ESP_LOGCONFIG(TAG, "%*sText Runs: %i", indent_depth, "", this->text_runs_.size());
  for (TextRunBase *run : this->text_runs_) {
    std::string text = run->get_text();
    ESP_LOGCONFIG(TAG, "%*sText: %s", indent_depth + additional_level_depth, "", text.c_str());
  }
}

void TextRunPanel::setup_complete() {
  if (!this->can_wrap_at_character_.has_value()) {
    ESP_LOGD(TAG, "No custom can_wrap_at_character provided. Will use default implementation");
    this->can_wrap_at_character_ = [this](const CanWrapAtCharacterArguments &args) {
      return this->default_can_wrap_at_character(args);
    };
  }
}

display::Rect TextRunPanel::measure_item_internal(display::Display *display) {
  CalculatedLayout calculated =
      this->determine_layout_(display, display::Rect(0, 0, this->max_width_, display->get_height()), true);
  return calculated.bounds;
}

void TextRunPanel::render_internal(display::Display *display, display::Rect bounds) {
  ESP_LOGD(TAG, "Rendering to (%i, %i)", bounds.w, bounds.h);

  CalculatedLayout layout = this->determine_layout_(display, bounds, true);

  for (const auto &calculated : layout.runs) {
    if (calculated->run->background_color_ != display::COLOR_OFF) {
      display->filled_rectangle(calculated->bounds.x, calculated->bounds.y, calculated->bounds.w, calculated->bounds.h,
                                calculated->run->background_color_);
    }
    display->print(calculated->bounds.x, calculated->bounds.y, calculated->run->font_,
                   calculated->run->foreground_color_, display::TextAlign::TOP_LEFT, calculated->text.c_str());
  }

  if (this->debug_outline_runs_) {
    ESP_LOGD(TAG, "Outlining character runs");
    for (const auto &calculated : layout.runs) {
      display->rectangle(calculated->bounds.x, calculated->bounds.y, calculated->bounds.w, calculated->bounds.h);
    }
  }
}

std::vector<std::shared_ptr<CalculatedTextRun>> TextRunPanel::split_runs_into_words_() {
  std::vector<std::shared_ptr<CalculatedTextRun>> runs;

  for (TextRunBase *run : this->text_runs_) {
    std::string text = run->get_text();
    CanWrapAtCharacterArguments can_wrap_at_args(this, 0, text, ' ');

    int last_break = 0;
    for (int i = 0; i < text.size(); i++) {
      can_wrap_at_args.character = text.at(i);
      can_wrap_at_args.offset = i;
      bool can_wrap = this->can_wrap_at_character_.value(can_wrap_at_args);
      if (!can_wrap) {
        continue;
      }

      auto calculated = std::make_shared<CalculatedTextRun>(run, text.substr(last_break, i - last_break));
      calculated->calculate_bounds();
      runs.push_back(calculated);
      last_break = i;
    }

    if (last_break < text.size()) {
      auto calculated = std::make_shared<CalculatedTextRun>(run, text.substr(last_break));
      calculated->calculate_bounds();
      runs.push_back(calculated);
    }
  }

  return runs;
}

std::vector<std::shared_ptr<LineInfo>> TextRunPanel::fit_words_to_bounds_(
    const std::vector<std::shared_ptr<CalculatedTextRun>> &runs, display::Rect bounds) {
  int x_offset = 0;
  int y_offset = 0;
  int current_line_number = 0;
  std::vector<std::shared_ptr<LineInfo>> lines;

  auto current_line = std::make_shared<LineInfo>(current_line_number);
  lines.push_back(current_line);

  for (const auto &run : runs) {
    if (run->bounds.w + x_offset > bounds.w) {
      // Overflows the current line create a new line
      x_offset = 0;
      y_offset += current_line->max_height;

      current_line_number++;
      current_line = std::make_shared<LineInfo>(current_line_number);

      lines.push_back(current_line);
    }

    // Fits on the line
    run->bounds.x = x_offset;
    run->bounds.y = y_offset;

    current_line->add_run(run);

    x_offset += run->bounds.w;
  }

  return lines;
}

void TextRunPanel::apply_alignment_to_lines_(std::vector<std::shared_ptr<LineInfo>> &lines,
                                             display::TextAlign alignment) {
  const auto x_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_X_MASK);
  const auto y_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_Y_MASK);

  int16_t max_line_width = 0;
  for (const auto &line : lines) {
    max_line_width = std::max(line->total_width, max_line_width);
  }

  int total_y_adjustment = 0;
  for (const auto &line : lines) {
    int x_adjustment = 0;
    int y_adjustment = 0;
    int max_line_y_adjustment = 0;

    // Horizontal alignment
    switch (x_align) {
      case display::TextAlign::RIGHT: {
        x_adjustment = max_line_width - line->total_width;
        break;
      }
      case display::TextAlign::CENTER_HORIZONTAL: {
        x_adjustment = (max_line_width - line->total_width) / 2;
        break;
      }
      case display::TextAlign::LEFT:
      default: {
        break;
      }
    }

    // Perform adjustment
    for (const auto &run : line->runs) {
      switch (y_align) {
        case display::TextAlign::BOTTOM: {
          y_adjustment = line->max_height - run->bounds.h;
          break;
        }
        case display::TextAlign::CENTER_VERTICAL: {
          y_adjustment = (line->max_height - run->bounds.h) / 2;
          break;
        }
        case display::TextAlign::BASELINE: {
          y_adjustment = line->max_baseline - run->baseline;
          break;
        }
        case display::TextAlign::TOP:
        default: {
          break;
        }
      }

      run->bounds.x += x_adjustment;
      run->bounds.y += y_adjustment + total_y_adjustment;
      max_line_y_adjustment = std::max(max_line_y_adjustment, y_adjustment);
    }

    total_y_adjustment += max_line_y_adjustment;
  }
}

CalculatedLayout TextRunPanel::determine_layout_(display::Display *display, display::Rect bounds,
                                                 bool apply_alignment) {
  std::vector<std::shared_ptr<CalculatedTextRun>> runs = this->split_runs_into_words_();
  std::vector<std::shared_ptr<LineInfo>> lines = this->fit_words_to_bounds_(runs, bounds);
  this->apply_alignment_to_lines_(lines, this->text_align_);

  CalculatedLayout layout;
  layout.runs = runs;
  layout.line_count = lines.size();

  int y_offset = 0;
  layout.bounds = display::Rect(0, 0, 0, 0);
  for (const auto &line : lines) {
    y_offset += line->max_height;
    layout.bounds.w = std::max(layout.bounds.w, line->total_width);
  }
  layout.bounds.h = y_offset;

  ESP_LOGD(TAG, "Text fits on %i lines and its bounds are (%i, %i)", layout.line_count, layout.bounds.w,
           layout.bounds.h);

  return layout;
}

bool TextRunPanel::default_can_wrap_at_character(const CanWrapAtCharacterArguments &args) {
  switch (args.character) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\0':
    case '=':
    case '<':
    case '>':
    case '/':
    case '&':
    case '*':
    case '+':
    case '^':
    case '|':
    case '\\': {
      return true;
    }
    default: {
      return false;
    }
  }

  return false;
}

}  // namespace graphical_layout
}  // namespace esphome

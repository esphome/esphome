#include "text_run_panel.h"
#include <memory>

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
  ESP_LOGCONFIG(TAG, "%*sDraw Partial Lines: %s", indent_depth, "", YESNO(this->draw_partial_lines_));
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

  CalculatedLayout layout = this->determine_layout_(display, bounds, false);
  int16_t y_offset = 0;

  display::Point offset = display->get_local_coordinates();
  display::Rect clipping_rect = display::Rect(offset.x, offset.y, bounds.w, bounds.h);
  display->start_clipping(clipping_rect);

  for (const auto &line : layout.lines) {
    if (!this->draw_partial_lines_ && ((y_offset + line->max_height) > bounds.h)) {
      ESP_LOGD(TAG, "Line %i would partially render outside of the area, skipping", line->line_number);
      continue;
    }

    for (const auto &calculated : line->runs) {
      if (!calculated->run_properties.is_printable) {
        continue;
      }
      if (calculated->run->background_color_ != display::COLOR_OFF) {
        display->filled_rectangle(calculated->bounds.x, calculated->bounds.y, calculated->bounds.w,
                                  calculated->bounds.h, calculated->run->background_color_);
      }
      display->print(calculated->bounds.x, calculated->bounds.y, calculated->run->font_,
                     calculated->run->foreground_color_, display::TextAlign::TOP_LEFT, calculated->text.c_str());
    }

    if (this->debug_outline_runs_) {
      ESP_LOGD(TAG, "Outlining character runs");
      for (const auto &calculated : line->runs) {
        display->rectangle(calculated->bounds.x, calculated->bounds.y, calculated->bounds.w, calculated->bounds.h);
      }
    }

    y_offset += line->max_height;
  }

  display->end_clipping();
}

std::vector<std::shared_ptr<CalculatedTextRun>> TextRunPanel::split_runs_into_words_() {
  std::vector<std::shared_ptr<CalculatedTextRun>> runs;
  for (TextRunBase *run : this->text_runs_) {
    std::string text = run->get_text();
    CanWrapAtCharacterArguments can_wrap_at_args(this, 0, text, ' ');

    std::shared_ptr<CalculatedTextRun> current_text_run = nullptr;
    for (int i = 0; i < text.size(); i++) {
      char current_char = text.at(i);
      CharacterProperties prop = this->get_character_properties_(current_char);
      can_wrap_at_args.character = current_char;
      can_wrap_at_args.offset = i;
      prop.can_wrap = this->can_wrap_at_character_.value(can_wrap_at_args);

      // New lines always trigger a new run regardless of equivalency with prior runs
      if ((current_text_run == nullptr) || (!current_text_run->run_properties.is_equivalent(prop)) ||
          (prop.causes_new_line)) {
        current_text_run = std::make_shared<CalculatedTextRun>(run, prop);
        runs.push_back(current_text_run);
      }

      if (prop.is_printable) {
        if (prop.replace_with.has_value()) {
          ESP_LOGD(TAG, "Replacing '%c' (0x%x) with '%s'", current_char, current_char,
                   prop.replace_with.value().c_str());
          current_text_run->text.append(prop.replace_with.value());
        } else {
          current_text_run->text.push_back(current_char);
        }
      }
    }
  }

  for (const auto &run : runs) {
    run->calculate_bounds();
  }

  return runs;
}

std::vector<std::shared_ptr<LineInfo>> TextRunPanel::fit_words_to_bounds_(
    const std::vector<std::shared_ptr<CalculatedTextRun>> &runs, display::Rect bounds, bool grow_beyond_bounds_height) {
  int x_offset = 0;
  int y_offset = 0;
  int current_line_number = 0;
  std::vector<std::shared_ptr<LineInfo>> lines;

  auto current_line = std::make_shared<LineInfo>(current_line_number);
  lines.push_back(current_line);

  bool is_first_run_of_line = true;

  for (const auto &run : runs) {
    if ((run->run_properties.causes_new_line) || (run->bounds.w + x_offset > bounds.w)) {
      // Overflows the current line create a new line
      x_offset = 0;
      y_offset += current_line->max_height;
      is_first_run_of_line = true;

      // Handle runs at the end of the line that want to be suppressed
      std::shared_ptr<CalculatedTextRun> last_run_of_line = current_line->runs.back();
      bool run_requires_recalculation = false;
      while (last_run_of_line->run_properties.suppress_at_end_of_line) {
        ESP_LOGD(TAG, "Suppressing run for '%s' as it's the end of a line", last_run_of_line->text.c_str());
        current_line->pop_last_run();

        last_run_of_line = current_line->runs.back();
        run_requires_recalculation = true;
      }
      if (run_requires_recalculation) {
        current_line->recalculate_line_measurements();
      }

      current_line_number++;
      current_line = std::make_shared<LineInfo>(current_line_number);

      lines.push_back(current_line);

      ESP_LOGD(TAG, "%i: Is New line: %s", current_line_number - 1, YESNO(run->run_properties.causes_new_line));
      ESP_LOGD(TAG, "Line %i finishes at %i vs available of %i", current_line_number - 1, y_offset, bounds.h);
      if (!grow_beyond_bounds_height && y_offset >= bounds.h) {
        ESP_LOGD(TAG, "No more text can fit into the available height. Aborting");
        break;
      }
    }

    // Fits on the line
    run->bounds.x = x_offset;
    run->bounds.y = y_offset;

    if (is_first_run_of_line && run->run_properties.suppress_at_start_of_line) {
      ESP_LOGD(TAG, "Suppressing run for '%s' as it's the start of a line", run->text.c_str());
      continue;
    }

    current_line->add_run(run);

    x_offset += run->bounds.w;
    is_first_run_of_line = false;
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
                                                 bool grow_beyond_bounds_height) {
  std::vector<std::shared_ptr<CalculatedTextRun>> runs = this->split_runs_into_words_();
  std::vector<std::shared_ptr<LineInfo>> lines = this->fit_words_to_bounds_(runs, bounds, grow_beyond_bounds_height);
  this->apply_alignment_to_lines_(lines, this->text_align_);

  CalculatedLayout layout;
  layout.lines = lines;

  int y_offset = 0;
  layout.bounds = display::Rect(0, 0, 0, 0);
  for (const auto &line : lines) {
    y_offset += line->max_height;
    layout.bounds.w = std::max(layout.bounds.w, line->total_width);
  }
  layout.bounds.h = y_offset;

  ESP_LOGD(TAG, "Text fits on %i lines and its bounds are (%i, %i)", layout.lines.size(), layout.bounds.w,
           layout.bounds.h);

  return layout;
}

inline CharacterProperties TextRunPanel::get_character_properties_(char character) {
  CharacterProperties props;
  props.character = character;

  if (character == '\t') {
    // Replace tabs with 4 spaces
    props.replace_with = std::string("    ");
    props.is_white_space = true;
    props.is_printable = true;
    props.suppress_at_end_of_line = false;
    props.suppress_at_start_of_line = false;
    return props;
  }

  // New line/Carriage Return are treated identically
  if ((character == '\n') || (character == '\r')) {
    props.is_white_space = true;
    // Don't display anything instead control new line with the causes_new_line
    props.replace_with = std::string("");
    props.causes_new_line = true;
    props.is_printable = false;

    return props;
  }

  // ASCII table is non-printable below space
  // 0x7f is the DEL character and the end of the normal ASCII set
  if ((character < ' ') || (character >= 0x7f)) {
    props.replace_with = std::string("");
    props.is_printable = false;
    return props;
  }

  if (character == ' ') {
    // Ensure we don't print at the start/end of the line. Wrapping to the next line is space enough
    props.suppress_at_end_of_line = true;
    props.suppress_at_start_of_line = true;
    props.is_printable = true;
    props.is_white_space = true;
    return props;
  }

  // Everything else should be printable
  props.is_printable = true;

  return props;
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

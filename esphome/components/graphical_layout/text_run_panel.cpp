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
  for (TextRun *run : this->text_runs_) {
    std::string text = run->text_.value();
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
      this->determine_layout(display, display::Rect(0, 0, this->max_width_, display->get_height()), true);
  return calculated.bounds;
}

void TextRunPanel::render_internal(display::Display *display, display::Rect bounds) {
  CalculatedLayout layout = this->determine_layout(display, bounds, true);

  for (const auto &calculated : layout.runs) {
    if (calculated->run->background_color_ != display::COLOR_OFF) {
      display->filled_rectangle(calculated->bounds.x, calculated->bounds.y, calculated->bounds.w, calculated->bounds.h,
                                calculated->run->background_color_);
    }
    display->print(calculated->bounds.x, calculated->bounds.y, calculated->run->font_,
                   calculated->run->foreground_color_, display::TextAlign::TOP_LEFT, calculated->text_.c_str());
  }

  if (this->debug_outline_runs_) {
    ESP_LOGD(TAG, "Outlining character runs");
    for (const auto &calculated : layout.runs) {
      display->rectangle(calculated->bounds.x, calculated->bounds.y, calculated->bounds.w, calculated->bounds.h);
    }
  }
}

CalculatedLayout TextRunPanel::determine_layout(display::Display *display, display::Rect bounds, bool apply_alignment) {
  ESP_LOGV(TAG, "Determining layout for (%i, %i)", bounds.w, bounds.h);

  CalculatedLayout calculated_layout;
  int x_offset = 0;
  int y_offset = 0;
  int current_line_max_height = 0;
  int widest_line = 0;
  int line_number = 0;

  for (TextRun *run : this->text_runs_) {
    int x1;
    int width;
    int height;
    int baseline;
    std::string text = run->text_.value();

    run->font_->measure(text.c_str(), &width, &x1, &baseline, &height);

    if ((x_offset + width) < bounds.w) {
      // Item fits on the current line
      auto calculated = std::make_shared<CalculatedTextRun>(run, text, display::Rect(x_offset, y_offset, width, height),
                                                            baseline, line_number);
      calculated_layout.runs.push_back(calculated);

      x_offset += width;
      widest_line = std::max(widest_line, x_offset);

      continue;
    }

    current_line_max_height = std::max(current_line_max_height, height);

    ESP_LOGVV(TAG, "'%s' will not fit on the line. Finding break characters", text.c_str());

    // Item extends beyond our desired bounds - need to add word by word
    CanWrapAtCharacterArguments can_wrap_at_args(this, 0, text, ' ');
    std::string partial_line;
    for (int i = 0; i < text.size(); i++) {
      can_wrap_at_args.offset = i;
      can_wrap_at_args.character = text.at(i);

      bool can_wrap = this->can_wrap_at_character_.value(can_wrap_at_args);
      if (can_wrap) {
        ESP_LOGVV(TAG, "Can break at '%c'. String is '%s'", can_wrap_at_args.character, partial_line.c_str());

        run->font_->measure(partial_line.c_str(), &width, &x1, &baseline, &height);
        if ((x_offset + width) < bounds.w) {
          ESP_LOGVV(TAG, "... Fits! (%i, %i)", x_offset, y_offset);

          // Item fits on the current line
          current_line_max_height = std::max(current_line_max_height, height);

          auto calculated = std::make_shared<CalculatedTextRun>(
              run, partial_line, display::Rect(x_offset, y_offset, width, height), baseline, line_number);
          calculated_layout.runs.push_back(calculated);

          x_offset += width;
          widest_line = std::max(widest_line, x_offset);

          partial_line = can_wrap_at_args.character;
          continue;
        }

        ESP_LOGVV(TAG, "... Doesn't fit - will overflow to next line");

        // Overflows the current line
        x_offset = 0;
        y_offset += current_line_max_height;
        line_number++;
        current_line_max_height = height;
        partial_line += can_wrap_at_args.character;
        continue;
      }

      partial_line += can_wrap_at_args.character;
    }

    if (partial_line.length() > 0) {
      // Remaining text
      run->font_->measure(partial_line.c_str(), &width, &x1, &baseline, &height);

      current_line_max_height = std::max(height, current_line_max_height);

      ESP_LOGVV(TAG, "'%s' is remaining after character break checks. Rendering to (%i, %i)", partial_line.c_str(),
                x_offset, y_offset);

      auto calculated = std::make_shared<CalculatedTextRun>(
          run, partial_line, display::Rect(x_offset, y_offset, width, height), baseline, line_number);
      calculated_layout.runs.push_back(calculated);

      x_offset += width;
      widest_line = std::max(widest_line, x_offset);
    }
  }

  y_offset += current_line_max_height;

  calculated_layout.bounds = display::Rect(0, 0, widest_line, y_offset);
  calculated_layout.line_count = line_number + 1;
  if (calculated_layout.bounds.w < this->min_width_) {
    calculated_layout.bounds.w = this->min_width_;
  }

  if (apply_alignment) {
    this->apply_alignment_to_layout(&calculated_layout);
  }

  ESP_LOGV(TAG, "Measured layout is (%i, %i) (%i lines)", calculated_layout.bounds.w, calculated_layout.bounds.h,
           calculated_layout.line_count);

  return calculated_layout;
}

void TextRunPanel::apply_alignment_to_layout(CalculatedLayout *calculated_layout) {
  const auto x_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_X_MASK);
  const auto y_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_Y_MASK);

  ESP_LOGVV(TAG, "We have %i lines to apply alignment to!", calculated_layout->line_count);

  int total_y_offset = 0;

  for (int i = 0; i < calculated_layout->line_count; i++) {
    std::vector<std::shared_ptr<CalculatedTextRun>> line_runs;

    // Get all the runs for the current line
    for (const auto &run : calculated_layout->runs) {
      if (run->line_number_ == i) {
        line_runs.push_back(run);
      }
    }

    ESP_LOGVV(TAG, "Found %i runs on line %i", line_runs.size(), i);

    int16_t total_line_width = 0;
    int16_t max_line_height = 0;
    int16_t max_baseline = 0;
    for (const auto &run : line_runs) {
      total_line_width += run->bounds.w;
      max_line_height = std::max(run->bounds.h, max_line_height);
      max_baseline = std::max(run->baseline, max_baseline);
    }

    ESP_LOGVV(TAG, "Line %i totals (%i, %i) pixels of (%i, %i)", i, total_line_width, max_line_height,
              calculated_layout->bounds.w, calculated_layout->bounds.h);

    int x_adjustment = 0;
    int y_adjustment = 0;
    switch (x_align) {
      case display::TextAlign::RIGHT: {
        x_adjustment = calculated_layout->bounds.w - total_line_width;
        ESP_LOGVV(TAG, "Will adjust line %i by %i x-pixels", i, x_adjustment);
        break;
      }
      case display::TextAlign::CENTER_HORIZONTAL: {
        x_adjustment = (calculated_layout->bounds.w - total_line_width) / 2;
        ESP_LOGVV(TAG, "Will adjust line %i by %i x-pixels", i, x_adjustment);
        break;
      }
      default: {
        break;
      }
    }

    int max_line_y_adjustment = 0;
    for (const auto &run : line_runs) {
      ESP_LOGVV(TAG, "Adjusting '%s' from (%i, %i) to (%i, %i)", run->text_.c_str(), run->bounds.x, run->bounds.y,
                run->bounds.x + x_adjustment, run->bounds.y + y_adjustment);
      run->bounds.x += x_adjustment;

      switch (y_align) {
        case display::TextAlign::BOTTOM: {
          y_adjustment = max_line_height - run->bounds.h;
          ESP_LOGVV(TAG, "Will adjust line %i by %i y-pixels (%i vs %i)", i, y_adjustment, max_line_height,
                    run->bounds.h);
          break;
        }
        case display::TextAlign::CENTER_VERTICAL: {
          y_adjustment = (max_line_height - run->bounds.h) / 2;
          ESP_LOGVV(TAG, "Will adjust line %i by %i y-pixels", i, y_adjustment);
          break;
        }
        case display::TextAlign::BASELINE: {
          // Adjust this run based on its difference from the maximum baseline in the line
          y_adjustment = max_baseline - run->baseline;
          ESP_LOGVV(TAG, "Will adjust '%s' by %i y-pixels (ML: %i, H: %i, BL: %i)", run->text_.c_str(), y_adjustment,
                    max_line_height, run->bounds.h, run->baseline);
          break;
        }
        default: {
          break;
        }
      }

      run->bounds.y += y_adjustment + total_y_offset;
      max_line_y_adjustment = std::max(max_line_y_adjustment, y_adjustment);
    }

    total_y_offset += max_line_y_adjustment;
  }

  calculated_layout->bounds.h += total_y_offset;
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

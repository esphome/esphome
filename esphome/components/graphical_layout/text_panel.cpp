#include "text_panel.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/rect.h"
#include "esphome/core/log.h"
#include <iomanip>
#include <sstream>

namespace esphome {
namespace graphical_layout {

static const char *const TAG = "textpanel";
static const int TEXT_ALIGN_X_MASK = (int) display::TextAlign::RIGHT | (int) display::TextAlign::CENTER_HORIZONTAL;
static const int TEXT_ALIGN_Y_MASK =
    (int) display::TextAlign::BOTTOM | (int) display::TextAlign::BASELINE | (int) display::TextAlign::CENTER_VERTICAL;

void TextPanel::dump_config(int indent_depth, int additional_level_depth) {
  this->dump_config_base_properties(TAG, indent_depth);
  std::string text = this->text_input_.value();
  ESP_LOGCONFIG(TAG, "%*sText Align: %s", indent_depth, "",
                LOG_STR_ARG(display::text_align_to_string(this->text_align_)));
  ESP_LOGCONFIG(TAG, "%*sText: %s", indent_depth, "", text.c_str());
  if (this->sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "%*sSensor: %s", indent_depth, "", this->sensor_->get_name().c_str());
  }
  if (this->text_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "%*sText Sensor: %s", indent_depth, "", this->text_sensor_->get_name().c_str());
  }
  if (this->time_ != nullptr) {
    ESP_LOGCONFIG(TAG, "%*sTime: YES", indent_depth, "");
    ESP_LOGCONFIG(TAG, "%*sUse UTC Time: %s", indent_depth, "", YESNO(this->use_utc_time_));
    ESP_LOGCONFIG(TAG, "%*sTime Format: %s", indent_depth, "", this->time_format_.value().c_str());
  }
  ESP_LOGCONFIG(TAG, "%*sHas Text Formatter: %s", indent_depth, "", YESNO(!this->text_formatter_.has_value()));
}

void TextPanel::setup_complete() {
  if (!this->text_formatter_.has_value()) {
    this->text_formatter_ = [](const std::string &string) { return string; };
  }

  if (this->sensor_ != nullptr) {
    // Need to setup the text callback for the sensor
    this->text_ = [this]() {
      std::stringstream stream;
      stream << std::fixed << std::setprecision(this->sensor_->get_accuracy_decimals()) << this->sensor_->get_state();
      return this->text_formatter_.value(stream.str());
    };
  }

  if (this->text_sensor_ != nullptr) {
    // Need to setup the text callback to the TextSensor
    this->text_ = [this]() { return this->text_formatter_.value(this->text_sensor_->get_state()); };
  }

  if (this->time_ != nullptr) {
    this->text_ = [this]() {
      ESPTime time;
      if (this->use_utc_time_) {
        time = this->time_->utcnow();
      } else {
        time = this->time_->now();
      }

      return this->text_formatter_.value(time.strftime(this->time_format_.value()));
    };
  }

  if (this->text_input_.has_value()) {
    this->text_ = [this]() { return this->text_formatter_.value(this->text_input_.value()); };
  }
}

display::Rect TextPanel::measure_item_internal(display::Display *display) {
  int x1;
  int y1;
  int width;
  int height;
  std::string text = this->text_.value();
  display->get_text_bounds(0, 0, text.c_str(), this->font_, this->text_align_, &x1, &y1, &width, &height);
  return display::Rect(0, 0, width, height);
}

void TextPanel::render_internal(display::Display *display, display::Rect bounds) {
  int width, height, x_offset, baseline;

  std::string text = this->text_.value();
  this->font_->measure(text.c_str(), &width, &x_offset, &baseline, &height);

  const auto x_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_X_MASK);
  const auto y_align = display::TextAlign(int(this->text_align_) & TEXT_ALIGN_Y_MASK);

  display::Rect text_bounds(0, 0, bounds.w, bounds.h);

  switch (x_align) {
    case display::TextAlign::RIGHT: {
      bounds.x = bounds.w - width;
      break;
    }
    case display::TextAlign::CENTER_HORIZONTAL: {
      bounds.x = (bounds.w - width) / 2;
      break;
    }
    case display::TextAlign::LEFT:
    default: {
      // LEFT
      bounds.x = 0;
      break;
    }
  }

  switch (y_align) {
    case display::TextAlign::BOTTOM: {
      bounds.y = bounds.h - height;
      break;
    }
    case display::TextAlign::BASELINE: {
      bounds.y = (bounds.h - height) + baseline;
      break;
    }
    case display::TextAlign::CENTER_VERTICAL: {
      bounds.y = (bounds.h - height) / 2;
      break;
    }
    case display::TextAlign::TOP:
    default: {
      bounds.y = 0;
      break;
    }
  }

  auto rendered_alignment = display::TextAlign::TOP_LEFT;
  display->print(bounds.x, bounds.y, this->font_, this->foreground_color_, rendered_alignment, text.c_str());
}

}  // namespace graphical_layout
}  // namespace esphome

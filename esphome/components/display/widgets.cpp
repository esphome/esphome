#include "widgets.h"

#include <utility>
#include "esphome/core/application.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

static const char* TAG = "display.widgets";

namespace esphome {
namespace display {

  static void add_clamp(int* dest, int a, int b) {
    if (__builtin_add_overflow(a, b, dest)) {
      *dest = INT_MAX;
    }
  }

  static int get_size(Widget::DimensionSource source, int user, int minimum, int preferred, int maximum) {
    if (user == Widget::AUTO || user < Widget::MAXIMUM) {
      user = source;
    }
    switch (user) {
    case Widget::MINIMUM:
      return minimum;
    case Widget::PREFERRED:
      return preferred;
    case Widget::MAXIMUM:
      return maximum;
    }
    return user;
  }

#define ASSIGN_SIZE(SOURCE, SOURCE_LOWER, DIRECTION) { *DIRECTION = get_size(Widget::SOURCE, user_ ## SOURCE_LOWER ## _ ## DIRECTION ## _, minimum_ ## DIRECTION ## _, preferred_ ## DIRECTION ## _, maximum_ ## DIRECTION ## _); }

#define GET_SIZE_FN(SOURCE, SOURCE_LOWER) void Widget::get_ ## SOURCE_LOWER ## _size(int *width, int *height) {\
  ASSIGN_SIZE(SOURCE, SOURCE_LOWER, width); \
  ASSIGN_SIZE(SOURCE, SOURCE_LOWER, height); \
}

  GET_SIZE_FN(MINIMUM, minimum);
  GET_SIZE_FN(PREFERRED, preferred);
  GET_SIZE_FN(MAXIMUM, maximum);

  template<typename T>
  static T sadd(T first, T second)
  {
    static_assert(std::is_integral<T>::value, "sadd is not defined for non-integral types");
    return first > std::numeric_limits<T>::max() - second ? std::numeric_limits<T>::max() : first + second;
  }

  /*
    All widgets need to declare their preferred size by implementing invalidate_layout.
    Container widgets need to calculate their preferred size from their child widgets' sizes.
  */

  void Widget::invalidate_layout() {
    // Defaults are fine.
  }

  void Widget::draw_fullscreen(DisplayBuffer& it) {
    // Trigger sizing
    invalidate_layout();
    ESP_LOGV(TAG, "Minimum size is (%d, %d)", minimum_width_, minimum_height_);
    ESP_LOGV(TAG, "Preferred size is (%d, %d)", preferred_width_, preferred_height_);

    it.fill(COLOR_ON);
    return draw(&it, 0, 0, it.get_width(), it.get_height());
  }

  Widget::SizeRequirements Widget::SizeRequirements::get_tiled_size_requirements(std::vector<SizeRequirements> children) {
    SizeRequirements total;
    for (auto& req : children) {
      add_clamp(&total.minimum, total.minimum, req.minimum);
      add_clamp(&total.preferred, total.preferred, req.preferred);
      add_clamp(&total.maximum, total.maximum, req.maximum);
    }
    return total;
  }

  Widget::SizeRequirements Widget::SizeRequirements::get_aligned_size_requirements(std::vector<SizeRequirements> children) {
    // This algorithm is taken from Swing's SizeRequirements class.
    SizeRequirements totalAscent;
    SizeRequirements totalDescent;
    for (auto& req : children) {
      int ascent = (int) (req.alignment * req.minimum);
      int descent = req.minimum - ascent;
      totalAscent.minimum = std::max(ascent, totalAscent.minimum);
      totalDescent.minimum = std::max(descent, totalDescent.minimum);

      ascent = (int) (req.alignment * req.preferred);
      descent = req.preferred - ascent;
      totalAscent.preferred = std::max(ascent, totalAscent.preferred);
      totalDescent.preferred = std::max(descent, totalDescent.preferred);

      ascent = (int) (req.alignment * req.maximum);
      descent = req.maximum - ascent;
      totalAscent.maximum = std::max(ascent, totalAscent.maximum);
      totalDescent.maximum = std::max(descent, totalDescent.maximum);
    }
    int min, pref, max;
    add_clamp(&min, totalAscent.minimum, totalDescent.minimum);
    add_clamp(&pref, totalAscent.preferred, totalDescent.preferred);
    add_clamp(&max, totalAscent.maximum, totalDescent.maximum);
    float alignment = 0.0f;
    if (min > 0) {
      alignment = (float) totalAscent.minimum / min;
      alignment = clamp(alignment, 0.0f, 1.0f);
    }
    Widget::SizeRequirements out;
    out.minimum = min;
    out.preferred = pref;
    out.maximum = max;
    out.alignment = alignment;
    return out;
  }

  void Widget::SizeRequirements::calculate_tiled_positions(int allocated, std::vector<SizeRequirements> children, std::vector<int> &offsets, std::vector<int> &spans) {
    offsets.resize(children.size());
    spans.resize(children.size());
    if (allocated >= preferred) {
      // Expanded tile
      float totalPlay = std::min(allocated - preferred, maximum - preferred);
      float factor = (maximum - preferred == 0) ? 0 : (totalPlay / (maximum - preferred));

      int totalOffset = 0;
      for (int i = 0; i < children.size(); i++) {
        offsets[i] = totalOffset;
        SizeRequirements &req = children[i];
        int play = (int)(factor * (req.maximum - req.preferred));
        add_clamp(&spans[i], req.preferred, play);
        add_clamp(&totalOffset, totalOffset, spans[i]);
      }
    } else {
      // Compressed tile
      float totalPlay = std::min(preferred - allocated, preferred - minimum);
      float factor = (preferred - minimum == 0) ? 0.0 : (totalPlay / (preferred - minimum));

      int totalOffset = 0;
      for (int i = 0; i < children.size(); i++) {
        offsets[i] = totalOffset;
        SizeRequirements &req = children[i];
        int play = (int)(factor * (req.preferred - req.minimum));
        spans[i] = req.preferred - play;
        add_clamp(&totalOffset, totalOffset, spans[i]);
      }
    }
  }
  void Widget::SizeRequirements::calculate_aligned_positions(int allocated, std::vector<SizeRequirements> children, std::vector<int> &offsets, std::vector<int> &spans) {
    offsets.resize(children.size());
    spans.resize(children.size());
    int totalAscent = (int)(allocated * alignment);
    int totalDescent = allocated - totalAscent;
    for (int i = 0; i < children.size(); i++) {
      SizeRequirements &req = children[i];
      int maxAscent = (int)(req.maximum * req.alignment);
      int maxDescent = req.maximum - maxAscent;
      int ascent = std::min(totalAscent, maxAscent);
      int descent = std::min(totalDescent, maxDescent);

      offsets[i] = totalAscent - ascent;
      add_clamp(&spans[i], ascent, descent);
    }
  }

  void WidgetContainer::invalidate_layout() {
    minimum_width_ = 0; minimum_height_ = 0;
    preferred_width_ = 0; preferred_height_ = 0;
    maximum_width_ = 0; maximum_height_ = 0;
    for (auto &child : children_) {
      child.widget_->invalidate_layout();
      int child_minimum_width, child_minimum_height;
      child.widget_->get_minimum_size(&child_minimum_width, &child_minimum_height);
      minimum_width_ = std::max(minimum_width_, child_minimum_width);
      minimum_height_ = std::max(minimum_height_, child_minimum_height);
      int child_preferred_width, child_preferred_height;
      child.widget_->get_preferred_size(&child_preferred_width, &child_preferred_height);
      preferred_width_ = std::max(preferred_width_, child_preferred_width);
      preferred_height_ = std::max(preferred_height_, child_preferred_height);
      int child_maximum_width, child_maximum_height;
      child.widget_->get_maximum_size(&child_maximum_width, &child_maximum_height);
      maximum_width_ = std::max(maximum_width_, child_maximum_width);
      maximum_height_ = std::max(maximum_height_, child_maximum_height);
    }
  }

  void WidgetContainer::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    for (auto child : children_) {
      child.widget_->draw(it, x1, y1, width, height);
    }
  }

  void Box::invalidate_layout() {
    xChildren_.resize(children_.size());
    yChildren_.resize(children_.size());
    for (int i = 0; i < children_.size(); i++) {
      Child &child = children_[i];
      child.widget_->invalidate_layout();
      child.widget_->get_minimum_size(&xChildren_[i].minimum, &yChildren_[i].minimum);
      child.widget_->get_preferred_size(&xChildren_[i].preferred, &yChildren_[i].preferred);
      child.widget_->get_maximum_size(&xChildren_[i].maximum, &yChildren_[i].maximum);
    }
    if (axis_ == X_AXIS) {
      xTotal_ = SizeRequirements::get_tiled_size_requirements(xChildren_);
      yTotal_ = SizeRequirements::get_aligned_size_requirements(yChildren_);
    } else {
      xTotal_ = SizeRequirements::get_aligned_size_requirements(xChildren_);
      yTotal_ = SizeRequirements::get_tiled_size_requirements(yChildren_);
    }
    minimum_width_ = xTotal_.minimum;
    preferred_width_ = xTotal_.preferred;
    maximum_width_ = xTotal_.maximum;
    minimum_height_ = yTotal_.minimum;
    preferred_height_ = yTotal_.preferred;
    maximum_height_ = yTotal_.maximum;
  }

  void Box::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    ESP_LOGV(TAG, "Box<%s>::draw at (%d, %d, %d, %d)", (axis_ == X_AXIS ? "Horizontal" : "Vertical"), x1, y1, width, height);
    std::vector<int> xOffsets, yOffsets, xSpans, ySpans;
    if (axis_ == X_AXIS) {
      xTotal_.calculate_tiled_positions(width, xChildren_, xOffsets, xSpans);
      yTotal_.calculate_aligned_positions(height, yChildren_, yOffsets, ySpans);
    } else {
      xTotal_.calculate_aligned_positions(width, xChildren_, xOffsets, xSpans);
      yTotal_.calculate_tiled_positions(height, yChildren_, yOffsets, ySpans);
    }
    for (int i = 0; i < children_.size(); i++) {
      ESP_LOGV(TAG, "Drawing child %d at (%d, %d, %d, %d)", i, xOffsets[i], yOffsets[i], xSpans[i], ySpans[i]);
      ESP_LOGV(TAG, "SizeRequirements x={%d, %d, %d, %g}, y={%d, %d, %d, %g}", xChildren_[i].minimum, xChildren_[i].preferred, xChildren_[i].maximum, xChildren_[i].alignment, yChildren_[i].minimum, yChildren_[i].preferred, yChildren_[i].maximum, yChildren_[i].alignment);
      children_[i].widget_->draw(it, x1+xOffsets[i], y1+yOffsets[i], xSpans[i], ySpans[i]);
    }
    ESP_LOGV(TAG, "Done drawing %d children", children_.size());
  }

  template<> void Text<>::calculate_text_() {
    std::string text = text_.value();
    if (source_ != NULL && source_->has_state()) {
      cached_text_.resize(256);
      snprintf(&cached_text_[0], cached_text_.size(), text.c_str(), source_->state);
    } else if (source_text_ != NULL && source_text_->has_state()) {
      cached_text_.resize(256);
      snprintf(&cached_text_[0], cached_text_.size(), text.c_str(), source_text_->state.c_str());
    } else {
      cached_text_ = text;
    }
  }
  template<> void Text<>::invalidate_layout() {
    calculate_text_();
    int unused_x_offset, unused_baseline;
    font_->measure(cached_text_.c_str(), &preferred_width_, &unused_x_offset, &unused_baseline, &preferred_height_);
    minimum_width_ = preferred_width_;
    minimum_height_ = preferred_height_;
  }

  template<> void Text<>::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    auto x_align = TextAlign(int(align_) & 0x18);
    auto y_align = TextAlign(int(align_) & 0x07);
    if (x_align == TextAlign::RIGHT) {
      x1 += width;
    } else if (x_align == TextAlign::CENTER_HORIZONTAL) {
      x1 += width/2;
    }
    if (y_align == TextAlign::BOTTOM) {
      y1 += height;
    } if (y_align == TextAlign::BASELINE) {
      y1 += (height*3)/4;
    } else if (y_align == TextAlign::CENTER_VERTICAL) {
      y1 += height/2;
    }
    it->print(x1, y1, font_, COLOR_OFF, align_, cached_text_.c_str());
  }

  const int BUTTON_BORDER_INNER = 10;
  const int BUTTON_BORDER_OUTER = 10;

  void Button::invalidate_layout() {
    child_->invalidate_layout();
    child_->get_minimum_size(&minimum_width_, &minimum_height_);
    child_->get_preferred_size(&preferred_width_, &preferred_height_);
    child_->get_maximum_size(&maximum_width_, &maximum_height_);
    minimum_width_ = sadd(minimum_width_, BUTTON_BORDER_INNER + BUTTON_BORDER_OUTER);
    minimum_height_ = sadd(minimum_height_, BUTTON_BORDER_INNER + BUTTON_BORDER_OUTER);
    preferred_width_ = sadd(preferred_width_, BUTTON_BORDER_INNER + BUTTON_BORDER_OUTER);
    preferred_height_ = sadd(preferred_height_, BUTTON_BORDER_INNER + BUTTON_BORDER_OUTER);
    maximum_width_ = sadd(maximum_width_, BUTTON_BORDER_INNER + BUTTON_BORDER_OUTER);
    maximum_height_ = sadd(maximum_height_, BUTTON_BORDER_INNER + BUTTON_BORDER_OUTER);
  }

  void Button::draw(DisplayBuffer* it, int x1, int y1, int width, int height) {
    int lx = x1+BUTTON_BORDER_OUTER+BUTTON_BORDER_INNER;
    int ty = y1+BUTTON_BORDER_OUTER+BUTTON_BORDER_INNER;
    int rx = x1+width-(BUTTON_BORDER_OUTER+BUTTON_BORDER_INNER);
    int by = y1+height-(BUTTON_BORDER_OUTER+BUTTON_BORDER_INNER);
    // Draw circles at each corner
    it->circle(lx, ty, BUTTON_BORDER_INNER, COLOR_OFF);
    it->circle(rx, ty, BUTTON_BORDER_INNER, COLOR_OFF);
    it->circle(lx, by, BUTTON_BORDER_INNER, COLOR_OFF);
    it->circle(rx, by, BUTTON_BORDER_INNER, COLOR_OFF);
    // Remove the unneeded parts of the circles
    it->filled_rectangle(lx, y1+BUTTON_BORDER_OUTER, rx-lx, height-(2*BUTTON_BORDER_OUTER), COLOR_ON);
    it->filled_rectangle(x1+BUTTON_BORDER_OUTER, ty, width-(2*BUTTON_BORDER_OUTER), by-ty, COLOR_ON);
    // Connect with lines
    it->line(lx, y1+BUTTON_BORDER_OUTER, rx, y1+BUTTON_BORDER_OUTER, COLOR_OFF);
    it->line(lx, y1+height-BUTTON_BORDER_OUTER, rx, y1+height-BUTTON_BORDER_OUTER, COLOR_OFF);
    it->line(x1+BUTTON_BORDER_OUTER, ty, x1+BUTTON_BORDER_OUTER, by, COLOR_OFF);
    it->line(x1+width-BUTTON_BORDER_OUTER, ty, x1+width-BUTTON_BORDER_OUTER, by, COLOR_OFF);
    // Draw content
    child_->draw(it, lx, ty, rx-lx, by-ty);
  }


}  // namespace display
}  // namespace esphome

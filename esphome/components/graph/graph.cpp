#include "graph.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "esphome/core/esphal.h"
#include <algorithm>
#include <sstream>
#include <iostream>  // std::cout, std::fixed
#include <iomanip>
namespace esphome {
namespace graph {

using namespace display;

static const char *const TAG = "graph";
static const char *const TAGL = "graphlegend";

void HistoryData::init(int length) {
  this->length_ = length;
  this->data_ = new (std::nothrow) float[length];
  if (this->data_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate HistoryData buffer!");
    return;
  }
  std::fill(data_, data_ + length_, NAN);
  this->last_sample_ = millis();
}
HistoryData::~HistoryData() { delete[](this->data_); }

void HistoryData::take_sample(float data) {
  if (data_ == nullptr)
    // init allocation failed
    return;
  uint32_t tm = millis();
  uint32_t dt = tm - last_sample_;
  last_sample_ = tm;

  // Step data based on time
  this->period_ += dt;
  while (this->period_ >= this->update_time_) {
    this->data_[this->count_] = data;
    this->period_ -= this->update_time_;
    this->count_ = (this->count_ + 1) % this->length_;
    ESP_LOGV(TAG, "Updating trace with value: %f", data);
  }
  if (!isnan(data)) {
    // Recalc recent max/min
    this->recent_min_ = data;
    this->recent_max_ = data;
    for (int i = 0; i < this->length_; i++) {
      if (!isnan(this->data_[i])) {
        if (this->recent_max_ < this->data_[i])
          this->recent_max_ = this->data_[i];
        if (this->recent_min_ > this->data_[i])
          this->recent_min_ = this->data_[i];
      }
    }
  }
}

void GraphTrace::init(Graph *g) {
  ESP_LOGI(TAG, "Init trace for sensor %s", this->get_name().c_str());
  this->data_.init(g->get_width());
  sensor_->add_on_state_callback([this](float state) { this->data_.take_sample(state); });
  this->data_.set_update_time_ms(g->get_duration() * 1000 / g->get_width());
}

void Graph::draw(DisplayBuffer *buff, uint16_t x_offset, uint16_t y_offset, Color color) {
  /// Plot border
  if (this->border_) {
    buff->horizontal_line(x_offset, y_offset, this->width_, color);
    buff->horizontal_line(x_offset, y_offset + this->height_ - 1, this->width_, color);
    buff->vertical_line(x_offset, y_offset, this->height_, color);
    buff->vertical_line(x_offset + this->width_ - 1, y_offset, this->height_, color);
  }
  /// Determine best y-axis scale and range
  float ymin = NAN;
  float ymax = NAN;
  for (auto *trace : traces_) {
    float mx = trace->get_tracedata()->get_recent_max();
    float mn = trace->get_tracedata()->get_recent_min();
    if (isnan(ymax) || (ymax < mx))
      ymax = mx;
    if (isnan(ymin) || (ymin > mn))
      ymin = mn;
  }
  // Adjust if manually overridden
  if (!isnan(this->min_value_))
    ymin = this->min_value_;
  if (!isnan(this->max_value_))
    ymax = this->max_value_;

  float yrange = ymax - ymin;
  if (yrange > this->max_range_) {
    // Look back in trace data to best-fit into local range
    float mx = NAN;
    float mn = NAN;
    for (int16_t i = 0; i < this->width_; i++) {
      for (auto *trace : traces_) {
        float v = trace->get_tracedata()->get_value(i);
        if (!isnan(v)) {
          if ((v - mn) > this->max_range_)
            break;
          if ((mx - v) > this->max_range_)
            break;
          if (isnan(mx) || (v > mx))
            mx = v;
          if (isnan(mn) || (v < mn))
            mn = v;
        }
      }
    }
    yrange = this->max_range_;
    if (!isnan(mn)) {
      ymin = mn;
      ymax = ymin + this->max_range_;
    }
    ESP_LOGV(TAG, "Graphing at max_range. Using local min %f, max %f", mn, mx);
  }

  float y_per_div = this->min_range_;
  if (!isnan(this->gridspacing_y_)) {
    y_per_div = this->gridspacing_y_;
  }
  // Restrict drawing too many gridlines
  if (yrange > 10 * y_per_div) {
    while (yrange > 10 * y_per_div) {
      y_per_div *= 2;
    }
    ESP_LOGW(TAG, "Graphing reducing y-scale to prevent too many gridlines");
  }

  // Adjust limits to nice y_per_div boundaries
  int yn = int(ymin / y_per_div);
  int ym = int(ymax / y_per_div) + int(1 * (fmodf(ymax, y_per_div) != 0));
  ymin = yn * y_per_div;
  ymax = ym * y_per_div;
  yrange = ymax - ymin;

  /// Draw grid
  if (!isnan(this->gridspacing_y_)) {
    for (int y = yn; y <= ym; y++) {
      int16_t py = (int16_t) roundf((this->height_ - 1) * (1.0 - (float) (y - yn) / (ym - yn)));
      for (int x = 0; x < this->width_; x += 2) {
        buff->draw_pixel_at(x_offset + x, y_offset + py, color);
      }
    }
  }
  if (!isnan(this->gridspacing_x_) && (this->gridspacing_x_ > 0)) {
    int n = this->duration_ / this->gridspacing_x_;
    // Restrict drawing too many gridlines
    if (n > 20) {
      while (n > 20) {
        n /= 2;
      }
      ESP_LOGW(TAG, "Graphing reducing x-scale to prevent too many gridlines");
    }
    for (int i = 0; i <= n; i++) {
      for (int y = 0; y < this->height_; y += 2) {
        buff->draw_pixel_at(x_offset + i * (this->width_ - 1) / n, y_offset + y, color);
      }
    }
  }

  /// Draw traces
  ESP_LOGV(TAG, "Updating graph. ymin %f, ymax %f", ymin, ymax);
  for (auto *trace : traces_) {
    Color c = trace->get_line_color();
    uint16_t thick = trace->get_line_thickness();
    for (int16_t i = 0; i < this->width_; i++) {
      float v = (trace->get_tracedata()->get_value(i) - ymin) / yrange;
      if (!isnan(v) && (thick > 0)) {
        int16_t x = this->width_ - 1 - i;
        uint8_t b = (i % (thick * LineType::PATTERN_LENGTH)) / thick;
        if (((uint8_t) trace->get_line_type() & (1 << b)) == (1 << b)) {
          int16_t y = (int16_t) roundf((this->height_ - 1) * (1.0 - v)) - thick / 2;
          for (int16_t t = 0; t < thick; t++) {
            buff->draw_pixel_at(x_offset + x, y_offset + y + t, c);
          }
        }
      }
    }
  }
}

/// Determine the best coordinates of drawing text + lines
void GraphLegend::init(Graph *g) {
  parent_ = g;

  // Determine maximum expected text and value width / height
  int txtw = 0, txtos = 0, txtbl = 0, txth = 0;
  int valw = 0, valos = 0, valbl = 0, valh = 0;
  int lt = 0;
  for (auto *trace : g->traces_) {
    std::string txtstr = trace->get_name();
    int fw, fos, fbl, fh;
    this->font_label->measure(txtstr.c_str(), &fw, &fos, &fbl, &fh);
    if (fw > txtw)
      txtw = fw;
    if (fh > txth)
      txth = fh;
    if (trace->get_line_thickness() > lt)
      lt = trace->get_line_thickness();
    ESP_LOGI(TAGL, "  %s %d %d", txtstr.c_str(), fw, fh);

    if (this->values_ != VALUE_POSITION_TYPE_NONE) {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(trace->sensor_->get_accuracy_decimals()) << trace->sensor_->get_state();
      std::string valstr = ss.str();
      if (this->units_) {
        valstr += trace->sensor_->get_unit_of_measurement();
      }
      this->font_value->measure(valstr.c_str(), &fw, &fos, &fbl, &fh);
      if (fw > valw)
        valw = fw;
      if (fh > valh)
        valh = fh;
      ESP_LOGI(TAGL, "    %s %d %d", valstr.c_str(), fw, fh);
    }
  }
  // Add extra margin
  txtw *= 1.2;
  valw *= 1.2;

  uint8_t n = g->traces_.size();
  uint16_t w = this->width_;
  uint16_t h = this->height_;
  DirectionType dir = this->direction_;
  ValuePositionType valpos = this->values_;
  if (!this->font_value) {
    valpos = VALUE_POSITION_TYPE_NONE;
  }
  // Line sample always goes below text for compactness
  this->yl = txth + (txth / 4) + lt / 2;

  if (dir == DIRECTION_TYPE_AUTO) {
    dir = DIRECTION_TYPE_HORIZONTAL;  // as default
    if (h > 0) {
      dir = DIRECTION_TYPE_VERTICAL;
    }
  }

  if (valpos == VALUE_POSITION_TYPE_AUTO) {
    // TODO: do something smarter?? - fit to w and h?
    valpos = VALUE_POSITION_TYPE_BELOW;
  }

  if (valpos == VALUE_POSITION_TYPE_BELOW) {
    this->yv = txth + (txth / 4);
    if (this->lines_)
      this->yv += txth / 4 + lt;
  } else if (valpos == VALUE_POSITION_TYPE_BESIDE) {
    this->xv = (txtw + valw) / 2;
  }

  // If width or height is specified we divide evenly within, else we do tight-fit
  if (w == 0) {
    this->x0 = txtw / 2;
    this->xs = txtw;
    if (valpos == VALUE_POSITION_TYPE_BELOW) {
      this->xs = std::max(txtw, valw);
      ;
      this->x0 = this->xs / 2;
    } else if (valpos == VALUE_POSITION_TYPE_BESIDE) {
      this->xs = txtw + valw;
    }
    if (dir == DIRECTION_TYPE_VERTICAL) {
      this->width_ = this->xs;
    } else {
      this->width_ = this->xs * n;
    }
  } else {
    this->xs = w / n;
    this->x0 = this->xs / 2;
  }

  if (h == 0) {
    this->ys = txth;
    if (valpos == VALUE_POSITION_TYPE_BELOW) {
      this->ys = txth + txth / 2 + valh;
      if (this->lines_) {
        this->ys += lt;
      }
    } else if (valpos == VALUE_POSITION_TYPE_BESIDE) {
      if (this->lines_) {
        this->ys = std::max(txth + txth / 4 + lt + txth / 4, valh + valh / 4);
      } else {
        this->ys = std::max(txth + txth / 4, valh + valh / 4);
      }
      this->height_ = this->ys * n;
    }
    if (dir == DIRECTION_TYPE_HORIZONTAL) {
      this->height_ = this->ys;
    } else {
      this->height_ = this->ys * n;
    }
  } else {
    this->ys = h / n;
  }

  if (dir == DIRECTION_TYPE_HORIZONTAL) {
    this->ys = 0;
  } else {
    this->xs = 0;
  }
}

void Graph::draw_legend(display::DisplayBuffer *buff, uint16_t x_offset, uint16_t y_offset, Color color) {
  if (!legend_)
    return;

  /// Plot border
  if (this->border_) {
    int w = legend_->width_;
    int h = legend_->height_;
    buff->horizontal_line(x_offset, y_offset, w, color);
    buff->horizontal_line(x_offset, y_offset + h - 1, w, color);
    buff->vertical_line(x_offset, y_offset, h, color);
    buff->vertical_line(x_offset + w - 1, y_offset, h, color);
  }

  int x = x_offset + legend_->x0;
  int y = y_offset;
  for (auto *trace : traces_) {
    std::string txtstr = trace->get_name();
    ESP_LOGV(TAG, "  %s", txtstr.c_str());

    buff->printf(x, y, legend_->font_label, trace->get_line_color(), TextAlign::TOP_CENTER, "%s", txtstr.c_str());

    if (legend_->lines_) {
      uint16_t thick = trace->get_line_thickness();
      for (int16_t i = 0; i < legend_->x0 * 4 / 3; i++) {
        uint8_t b = (i % (thick * LineType::PATTERN_LENGTH)) / thick;
        if (((uint8_t) trace->get_line_type() & (1 << b)) == (1 << b)) {
          buff->vertical_line(x - legend_->x0 * 2 / 3 + i, y + legend_->yl - thick / 2, thick, trace->get_line_color());
        }
      }
    }

    if (legend_->values_ != VALUE_POSITION_TYPE_NONE) {
      int xv = x + legend_->xv;
      int yv = y + legend_->yv;
      std::stringstream ss;
      ss << std::fixed << std::setprecision(trace->sensor_->get_accuracy_decimals()) << trace->sensor_->get_state();
      std::string valstr = ss.str();
      if (legend_->units_) {
        valstr += trace->sensor_->get_unit_of_measurement();
      }
      buff->printf(xv, yv, legend_->font_value, trace->get_line_color(), TextAlign::TOP_CENTER, "%s", valstr.c_str());
      ESP_LOGV(TAG, "    value: %s", valstr.c_str());
    }
    x += legend_->xs;
    y += legend_->ys;
  }
}

void Graph::setup() {
  for (auto *trace : traces_) {
    trace->init(this);
  }
}

void Graph::dump_config() {
  for (auto *trace : traces_) {
    ESP_LOGCONFIG(TAG, "Graph for sensor %s", trace->get_name().c_str());
  }
}

}  // namespace graph
}  // namespace esphome

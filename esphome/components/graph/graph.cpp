#include "graph.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/core/log.h"

namespace esphome {
namespace graph {

using namespace display;

static const char *const TAG = "graph";

HistoryData::HistoryData(int length) : length_(length) {
  this->data_ = new float[length];
  if (this->data_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate HistoryData buffer!");
    return;
  }
  this->last_sample_ = millis();
  for (int i = 0; i < this->length_; i++)
    this->data_[i] = NAN;
}
HistoryData::~HistoryData() { delete (this->data_); }

void HistoryData::take_sample(float data) {
  uint64_t tm = millis();
  uint32_t dt = tm - last_sample_;
  last_sample_ = tm;

  // Step data based on time
  this->period_ += dt;
  while (this->period_ >= this->update_time_) {
    this->data_[this->count_] = data;
    this->period_ -= this->update_time_;
    this->count_ = (this->count_ + 1) % this->length_;
    ESP_LOGV(TAG, "Updating graph with value: %f", data);
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
void HistoryData::set_update_time_ms(uint32_t u) { this->update_time_ = u; }

void Graph::draw(DisplayBuffer *buff, uint16_t x_offset, uint16_t y_offset, Color color) {
  /// Plot border
  if (this->border_) {
    buff->horizontal_line(x_offset, y_offset, this->width_, color);
    buff->horizontal_line(x_offset, y_offset + this->height_ - 1, this->width_, color);
    buff->vertical_line(x_offset, y_offset, this->height_, color);
    buff->vertical_line(x_offset + this->width_ - 1, y_offset, this->height_, color);
  }
  /// Determine best y-axis scale and range
  float ymin = this->data_->get_recent_min();
  float ymax = this->data_->get_recent_max();

  // Adjust if manually overridden
  if (!isnan(this->min_value_))
    ymin = this->min_value_;
  if (!isnan(this->max_value_))
    ymax = this->max_value_;

  float yrange = ymax - ymin;
  if (yrange > this->max_range_) {
    // Look back in data to fit into local range
    float mx = NAN;
    float mn = NAN;
    for (int16_t i = 0; i < this->data_->get_length(); i++) {
      float v = this->data_->get_value(i);
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
        buff->draw_pixel_at(x_offset + x, y_offset + py);
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
        buff->draw_pixel_at(x_offset + i * (this->width_ - 1) / n, y_offset + y);
      }
    }
  }
  ESP_LOGI(TAG, "Updating graph. Last sample %f, ymin %f, ymax %f", this->data_->get_value(0), ymin, ymax);
  /// Draw data trace
  for (int16_t i = 0; i < this->data_->get_length(); i++) {
    float v = (this->data_->get_value(i) - ymin) / yrange;
    if (!isnan(v) && (this->line_thickness_ > 0)) {
      int16_t x = this->width_ - 1 - i;
      uint8_t b = (i % (this->line_thickness_ * LineType::PATTERN_LENGTH)) / this->line_thickness_;
      if ((this->line_type_ & (1 << b)) == (1 << b)) {
        int16_t y = (int16_t) roundf((this->height_ - 1) * (1.0 - v)) - this->line_thickness_ / 2;
        for (int16_t t = 0; t < this->line_thickness_; t++) {
          buff->draw_pixel_at(x_offset + x, y_offset + y + t);
        }
      }
    }
  }
}
void Graph::set_sensor(sensor::Sensor *sensor) {
  this->sensor_ = sensor;
  sensor->add_on_state_callback([this](float state) { this->data_->take_sample(state); });
  this->data_->set_update_time_ms(this->duration_ * 1000 / this->width_);
}
Graph::Graph(uint32_t duration, int width, int height) : duration_(duration), width_(width), height_(height) {
  this->data_ = new HistoryData(width);
}

}  // namespace graph
}  // namespace esphome

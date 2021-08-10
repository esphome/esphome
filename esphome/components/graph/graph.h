#pragma once
#include <cstdint>
#include "esphome/core/color.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/display/types.h"

namespace esphome {

// forward declare DisplayBuffer
namespace display {
class DisplayBuffer;
}  // namespace display

namespace graph {

class HistoryData {
 public:
  HistoryData(int length);
  ~HistoryData();
  void set_update_time_ms(uint32_t u);
  void take_sample(float data);
  int get_length() const { return length_; }
  float get_value(int idx) const { return data_[(count_ + length_ - 1 - idx) % length_]; }
  float get_recent_max() const { return recent_max_; }
  float get_recent_min() const { return recent_min_; }

 protected:
  uint64_t last_sample_;
  uint32_t period_{0};       /// in ms
  uint32_t update_time_{0};  /// in ms
  int length_;
  int count_{0};
  float recent_min_{NAN};
  float recent_max_{NAN};
  float *data_;
};

class Graph {
 public:
  Graph(uint32_t duration, int width, int height);
  void draw(display::DisplayBuffer *buff, uint16_t x_offset, uint16_t y_offset, Color color);
  void set_sensor(sensor::Sensor *sensor);
  void set_min_value(float val) { this->min_value_ = val; }
  void set_max_value(float val) { this->max_value_ = val; }
  void set_min_range(float val) { this->min_range_ = val; }
  void set_max_range(float val) { this->max_range_ = val; }
  void set_line_thickness(uint8_t val) { this->line_thickness_ = val; }
  void set_line_type(uint8_t val) { this->line_type_ = val; }
  void set_grid_x(float val) { this->gridspacing_x_ = val; }
  void set_grid_y(float val) { this->gridspacing_y_ = val; }
  void set_border(bool val) { this->border_ = val; }

 protected:
  int duration_;  /// in seconds
  int width_;     /// in pixels
  int height_;    /// in pixels
  float min_value_{NAN};
  float max_value_{NAN};
  float min_range_{1.0};
  float max_range_{NAN};
  uint8_t line_thickness_{3};
  uint8_t line_type_{display::LINE_TYPE_SOLID};
  float gridspacing_x_{NAN};
  float gridspacing_y_{NAN};
  bool border_{true};
  sensor::Sensor *sensor_{nullptr};  // TODO: Used??
  HistoryData *data_;
};

}  // namespace graph
}  // namespace esphome

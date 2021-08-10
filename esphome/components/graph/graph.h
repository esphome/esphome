#pragma once
#include <cstdint>
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {

// forward declare DisplayBuffer
namespace display {
class DisplayBuffer;
}  // namespace display

namespace graph {

/// Bit pattern defines the line-type
enum LineType {
  LINE_TYPE_SOLID = 0b1111,
  LINE_TYPE_DOTTED = 0b0101,
  LINE_TYPE_DASHED = 0b0111,
  // Following defines number of bits used to define line pattern
  PATTERN_LENGTH = 4
};

class HistoryData {
 public:
  void init(int length);
  ~HistoryData();
  void set_update_time_ms(uint32_t update_time_ms) { update_time_ = update_time_ms; }
  void take_sample(float data);
  int get_length() const { return length_; }
  float get_value(int idx) const { return data_[(count_ + length_ - 1 - idx) % length_]; }
  float get_recent_max() const { return recent_max_; }
  float get_recent_min() const { return recent_min_; }

 protected:
  uint32_t last_sample_;
  uint32_t period_{0};       /// in ms
  uint32_t update_time_{0};  /// in ms
  int length_;
  int count_{0};
  float recent_min_{NAN};
  float recent_max_{NAN};
  float *data_ = nullptr;
};

class Graph : public Component {
 public:
  void draw(display::DisplayBuffer *buff, uint16_t x_offset, uint16_t y_offset, Color color);
  void setup() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void dump_config() override;

  void set_duration(uint32_t duration) { duration_ = duration; }
  void set_width(uint32_t width) { width_ = width; }
  void set_height(uint32_t height) { height_ = height; }
  void set_sensor(sensor::Sensor *sensor) { sensor_ = sensor; }
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
  uint32_t duration_;  /// in seconds
  uint32_t width_;     /// in pixels
  uint32_t height_;    /// in pixels
  float min_value_{NAN};
  float max_value_{NAN};
  float min_range_{1.0};
  float max_range_{NAN};
  uint8_t line_thickness_{3};
  uint8_t line_type_{LINE_TYPE_SOLID};
  float gridspacing_x_{NAN};
  float gridspacing_y_{NAN};
  bool border_{true};
  sensor::Sensor *sensor_{nullptr};
  HistoryData data_;
};

}  // namespace graph
}  // namespace esphome

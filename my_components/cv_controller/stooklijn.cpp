#include "stooklijn.h"

namespace esphome {
namespace cv_controller {

Stooklijn::Stooklijn() {
  pref_ = global_preferences->make_preference<StooklinePoints>(1, 78952);
  memset(&stookline_, 1, sizeof(this->stookline_));
}

void Stooklijn::setup() {
  if (load() == false) {
    fillInitialStookline();
    bend(9.3, 50.0);  // starting estimate
    save();           // save the initial stooklijn to preferene memory
  }
}

void Stooklijn::save() {
  pref_.save(&stookline_);
  if (debug_) {
    ESP_LOGI(TAG, "Stooklijn points saved to flash.");
  }
}

bool Stooklijn::load() {
  if (!pref_.load(&stookline_)) {
    if (debug_) {
      ESP_LOGI(TAG, "No saved stooklijn points.");
    }
    return false;
  }
  if (debug_) {
    ESP_LOGI(TAG, "Stooklijn points  loaded from flash.");
  }
  return true;
}
void Stooklijn::bend(float outside_temperature, float aanvoer_temperature) {
  // replace the old aanvoer_temperature for this outside_temperture with the new one
  // move all points left and right from this outside_temperature with a liniear factor from one to zero
  float old_aanvoer_temperature = getAanvoerTemperature(outside_temperature);
  float delta = aanvoer_temperature - old_aanvoer_temperature;
  int index = getStookLineIndex(outside_temperature);

  setAanvoerTemperature(index, aanvoer_temperature);  // change the selected point

  if (index > 1) {
    // we need to linear adapt the stooklijn points, below the index
    // change the values for the lower index points
    // don't change the first point of the array (is min value anyway)

    float lin_delta_low = delta / index;
    for (int i = 1; i < index; i++) {
      float current_aanvoer = getAanvoerTemperature(i);
      setAanvoerTemperature(i, current_aanvoer + lin_delta_low * i);
    }
  }

  if (index < stookline_dp_points_ - 2) {
    // we need to linear adapt the stooklijn points, above the index
    // change the values for the higher index points
    // don't change the last point in the areay (is max value anyway)
    float lin_delta_high = delta / (stookline_dp_points_ - 1 - index);
    for (int i = stookline_dp_points_ - 2; i > index; i--) {
      float current_aanvoer = getAanvoerTemperature(i);
      setAanvoerTemperature(i, current_aanvoer + lin_delta_high * ((stookline_dp_points_ - 1) - i));
    }
  }
}

void Stooklijn::adaptStookLine(float delta_temperature, float outside_temperature) {
  float old_aanvoer_temperature = getAanvoerTemperature(outside_temperature);
  float new_aanvoer_temperature = old_aanvoer_temperature + delta_temperature;
  bend(outside_temperature, new_aanvoer_temperature);
}

float Stooklijn::performScaling(float input, float min_input, float max_input, float min_output, float max_output) {
  // store contains values between min_store to max_store
  // this maps to min_input and max_input
  // use the input to calculate the corresponding output

  if (input < min_input)
    return min_output;
  if (input > max_input)
    return max_output;
  return (input - min_input) * (max_output - min_output) / (max_input - min_input) + min_output;
}

int Stooklijn::getStookLineIndex(float outside_temperature) {
  float scaled_outside_temp = performScaling(outside_temperature, min_outside_temperature_, max_outside_temperature_,
                                             0.0, (float) stookline_dp_points_);
  int index = round(scaled_outside_temp);
  if (index < 0)
    index = 0;
  if (index >= stookline_dp_points_)
    index = stookline_dp_points_ - 1;
  return index;
}

float Stooklijn::getAanvoerTemperature(float outside_temperature) {
  int index = getStookLineIndex(outside_temperature);
  return getAanvoerTemperature(index);
}

float Stooklijn::getAanvoerTemperature(int index) {
  float raw_control_temp = (float) stookline_.dp[index];  // returns a value between 0 and 255 converts to float
  return performScaling(raw_control_temp, (float) 0, (float) 255, min_aanvoer_temperature_, max_aanvoer_temperature_);
}

float Stooklijn::getOutsideTemperature(int index) {
  if (index < 0)
    index = 0;
  if (index >= stookline_dp_points_)
    index = stookline_dp_points_ - 1;
  return performScaling((float) index, (float) 0, (float) stookline_dp_points_, min_outside_temperature_,
                        max_outside_temperature_);
}

void Stooklijn::setAanvoerTemperature(float outside_temperature, float aanvoer_temperature) {
  int index = getStookLineIndex(outside_temperature);
  setAanvoerTemperature(index, aanvoer_temperature);
}

void Stooklijn::setAanvoerTemperature(int index, float aanvoer_temperature) {
  stookline_.dp[index] = (uint8_t) round(
      performScaling(aanvoer_temperature, min_aanvoer_temperature_, max_aanvoer_temperature_, (float) 0, float(255)));
}

void Stooklijn::fillInitialStookline() {
  float outside_temperature_step = (max_outside_temperature_ - min_outside_temperature_) / stookline_dp_points_;
  float aanvoer_temperature_step = (max_aanvoer_temperature_ - min_aanvoer_temperature_) / stookline_dp_points_;
  for (int i = 0; i < stookline_dp_points_; i++) {
    setAanvoerTemperature(min_outside_temperature_ + i * outside_temperature_step,
                          max_aanvoer_temperature_ - i * aanvoer_temperature_step);
  }
}

void Stooklijn::printStookLine() {
  // maxium buffer size for a log is 512 bytes
  // so we push the raw bytes out and process the result (a string) in the spreadsheet
  std::string line;
  line.append("\nData;\n");

  for (int i = 0; i < stookline_dp_points_; i++) {
    uint8_t datapoint = stookline_.dp[i];
    line.append(to_string(datapoint));
    if (i < stookline_dp_points_ - 1) {
      line.append(";\n");
    }
  }

  if (debug_) {
    ESP_LOGI(TAG, "%s", line.c_str());
  }
}

}  // namespace cv_controller
}  // namespace esphome

#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace cv_controller {
using namespace std;

struct StooklinePoints {  // we need this because preferences can't save a uint8_t[100] array directly
  uint8_t dp[100];
};

// CAUTION create only one instance of this class, due to the preference object
class Stooklijn {
 public:
  Stooklijn();
  void setup();
  void save();
  bool load();
  float getAanvoerTemperature(float outside_temperature);
  void bend(float outside_temperture, float aanvoer_temperature);
  void adaptStookLine(float delta_temperature, float outside_temperature);
  void printStookLine();

 protected:
  const char *TAG = "Stooklijn";
  bool debug_ = false;
  static const int stookline_dp_points_ = 100;
  StooklinePoints stookline_;  // contains the desired control temperatuur for a range of outside temperatures
                               // if the control temperature gets below the 45 degrees, the old radiators don't produce
                               // enough heat, so minium is 45 degrees maxium is 85 degrees (burner maxium is 87)

  const float max_outside_temperature_ = 15;  // instelling voetpunt outside temp waar de stooklijn begint
  const float min_outside_temperature_ =
      -10;  // the minium temperature where the stooklijn reaches max aanvoer temperature
  const float max_aanvoer_temperature_ = 87;  // maxium aanvoer temperature when the outside temperature is very low
  const float min_aanvoer_temperature_ =
      25;  // minium aanvoer temperature when the outside temperature is high (voetpunt)

  float performScaling(float input, float min_input, float max_input, float min_store, float max_store);
  int getStookLineIndex(float outside_temperature);

  float getAanvoerTemperature(int index);
  float getOutsideTemperature(int index);
  void setAanvoerTemperature(float outside_temperature, float aanvoer_temperature);
  void setAanvoerTemperature(int index, float aanvoer_temperature);
  void fillInitialStookline();

 private:
  ESPPreferenceObject pref_;
};

}  // namespace cv_controller
}  // namespace esphome

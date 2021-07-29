#pragma once
#include "esphome/components/midea_dongle/midea_frame.h"
#include "esphome/components/climate/climate_traits.h"

namespace esphome {
namespace midea_ac {

using midea_dongle::Frame;
using climate::ClimateTraits;
using climate::ClimateMode;
using climate::ClimatePreset;
using climate::ClimateSwingMode;
using climate::ClimateFanMode;

class Capabilities {
 public:
  static const std::string FREEZE_PROTECTION;
  static const std::string SILENT;
  static const std::string TURBO;

  // Read from frames
  bool read(const Frame &frame);
  // Is ready for reading Climate traits
  bool is_ready() const { return this->is_ready_; }
  void dump(const char *tag) const;
  // To ClimateTraits
  void to_climate_traits(ClimateTraits &traits) const;
  // Control humidity
  bool auto_set_humidity() const { return this->auto_set_humidity_; };
  bool auto_mode() const { return this->auto_mode_; };
  bool active_clean() const { return this->active_clean_; };
  bool breeze_control() const { return this->breeze_control_; };
  bool buzzer() const { return this->buzzer_; }
  bool cool_mode() const { return this->cool_mode_; }
  bool decimals() const { return this->decimals_; }
  bool dry_mode() const { return this->dry_mode_; }
  bool eco_mode() const { return this->eco_mode_; }
  bool electric_aux_heating() const { return this->electric_aux_heating_; }
  bool fanspeed_control() const { return this->fanspeed_control_; }
  bool frost_protection_mode() const { return this->frost_protection_mode_; }
  bool heat_mode() const { return this->heat_mode_; }
  bool indoor_humidity() const { return this->indoor_humidity_; }
  bool leftright_fan() const { return this->leftright_fan_; }
  // Ability to turn LED display off
  bool light_control() const { return this->light_control_; }
  // Control humidity
  bool manual_set_humidity() const { return this->manual_set_humidity_; }
  float max_temp_auto() const { return this->max_temp_auto_; }
  float max_temp_cool() const { return this->max_temp_cool_; }
  float max_temp_heat() const { return this->max_temp_heat_; }
  float min_temp_auto() const { return this->min_temp_auto_; }
  float min_temp_cool() const { return this->min_temp_cool_; }
  float min_temp_heat() const { return this->min_temp_heat_; }
  bool nest_check() const { return this->nest_check_; }
  bool nest_need_change() const { return this->nest_need_change_; }
  bool one_key_no_wind_on_me() const { return this->one_key_no_wind_on_me_; }
  bool power_cal() const { return this->power_cal_; }
  bool power_cal_setting() const { return this->power_cal_setting_; }
  bool silky_cool() const { return this->silky_cool_; }
  // Intelligent eye function
  bool smart_eye() const { return this->smart_eye_; }
  bool sound() const { return this->sound_; }
  bool special_eco() const { return this->special_eco_; }
  bool turbo_cool() const { return this->turbo_cool_; }
  bool turbo_heat() const { return this->turbo_heat_; }
  // Temperature unit can be changed between Celsius and Fahrenheit
  bool unit_changeable() const { return this->unit_changeable_; }
  bool updown_fan() const { return this->updown_fan_; }
  bool wind_of_me() const { return this->wind_of_me_; }
  bool wind_on_me() const { return this->wind_on_me_; }
 protected:
  bool auto_set_humidity_{false};
  bool auto_mode_{false};
  bool active_clean_{false};
  bool breeze_control_{false};
  bool buzzer_{false};
  bool cool_mode_{false};
  bool decimals_{false};
  bool dry_mode_{false};
  bool eco_mode_{false};
  bool electric_aux_heating_{false};
  bool fanspeed_control_{true};
  bool frost_protection_mode_{false};
  bool heat_mode_{false};
  bool indoor_humidity_{false};
  bool leftright_fan_{false};
  bool light_control_{false};
  bool manual_set_humidity_{false};
  float max_temp_auto_{30};
  float max_temp_cool_{30};
  float max_temp_heat_{30};
  float min_temp_auto_{17};
  float min_temp_cool_{17};
  float min_temp_heat_{17};
  bool nest_check_{false};
  bool nest_need_change_{false};
  bool one_key_no_wind_on_me_{false};
  bool power_cal_{false};
  bool power_cal_setting_{false};
  bool silky_cool_{false};
  bool smart_eye_{false};
  bool sound_{false};
  bool special_eco_{false};
  bool turbo_cool_{false};
  bool turbo_heat_{false};
  bool unit_changeable_{false};
  bool updown_fan_{false};
  bool wind_of_me_{false};
  bool wind_on_me_{false};
  // is filled?
  bool is_ready_{false};
};

#if 0
enum DeviceID : int8_t {
  ID_INVALID = -1,
  ID_QA100,
  ID_SA100,
  ID_SA200,
  ID_SA300,
  ID_YA,
  ID_26YA,
  ID_YAB3,
  ID_WJABC,
  ID_YA100,
  ID_CJ200,
  ID_WYA,
  ID_WPA,
  ID_LBB2,
  ID_LBB3,
  ID_PA400,
  ID_WEA, // children air conditioning
  ID_CA,
  ID_TAB3,
  ID_TAB12,
  ID_KHB1,
  ID_CACP,
  ID_ZA300,
  ID_ZA300B3,
  ID_ZB300,
  ID_ZB300YJ,
  ID_YA300,
  ID_YA201,
  ID_YA200,
  ID_YA301,
  ID_YA302,
  ID_PC400B23,
  ID_WXA,
  ID_WJ7,
  ID_WYB,
  ID_PCB50,
  ID_YB200,
  ID_DA100,
  ID_YB200TEST,
  ID_WXD,
  ID_YB300,
  ID_WDAA3,
  ID_WDAD3,
  ID_WYAD2D3,
  ID_WPAD3,
  ID_DA400B3,
  ID_PC400B1,
  ID_DA200300,
  ID_DA400D3,
  ID_DA400BP,
  ID_DA400DP,
  ID_PA400B3,
  ID_J9,
  ID_YA400D2D3,
  ID_QA301B1,
  ID_WCBA3,
  ID_IQ100,
  ID_DA100Z,
  ID_IQ300,
  ID_J7,
  ID_WPBC,
  ID_WPCD,
  ID_YB400,
  ID_PE400B3,
  ID_J8,
  ID_YA400B2,
  ID_YB301,
  ID_WYS,
  ID_1TO1,
  ID_1TON,
  ID_YB201,
  ID_PF200,
  ID_GM100,
  ID_WOW,
  ID_S10,
  ID_FA100,
  ID_FA200,
  ID_W10,
  ID_WXDF,
  ID_YB100,
  ID_MQ200,
  ID_GW10,
  ID_OTHER
};

struct DeviceInfo {
  static DeviceInfo fromSN(const char *sn);
  char id[16] = {'\0'};
  // define the air-conditioned property
  DeviceID deviceType{DeviceID::ID_INVALID};
  // the new Protocol initiative to report
  bool hasNewProtocolAutoUpload{false};
  // just Protocol initiative to report
  bool hasOldProtocolAutoUpload{true};
  // Refrigeration
  bool hasRefrigerantCheck{true};
  // heating
  bool hasHotMode{true};
  // send the winds
  bool hasWindMode{true};
  // automatically
  bool hasAutoMode{true};
  // dehumidifier
  bool hasDryMode{true};
  // comfort dehumidifier
  bool hasSmartRemoveWet{false};
  // personality pumping wet
  bool hasManualRemoveWet{false};
  // 0.5 degree control
  bool hasDot5Support{false};
  // stepless
  bool hasNoPolar{true};
  // outdoor temperature display
  bool hasOuterDoorDisplay{true};
  // indoor humidity display
  bool hasHumidityDisplay{false};
  // put on the air to
  bool hasUpSwipeWind{false};
  // hem wind of
  bool hasDownSwipeWind{false};
  // up and down swing of the wind
  bool hasUpDownSwipeWind{false};
  // from left to right wind of
  bool hasLeftRightSwipeWind{false};
  // put the wind
  bool hasSwipeWind{false};
  // ECO
  bool hasECO{false};
  // dried
  bool hasDry{true};
  // electric auxiliary heat
  bool hasElectricHeat{true};
  // purge
  bool hasPurify{false};
  // the natural wind of
  bool hasNatureWind{false};
  // child anti-catch
  bool hasChildrenPreventCold{false};
  // pre-cooling preheating
  bool hasReadyColdOrHot{false};
  // no wind sense of
  bool hasNoWindFeel{false};
  // PMV
  bool hasPMV{false};
  // saving
  bool hasSavingPower{false};
  // PM2. 5detecting
  bool hasPurifyCheck{false};
  // comfortable sleep curve
  bool hasSleepCurve{false};
  // millet curve
  bool hasXiaomiSmartCurve{false};
  // millet bracelet
  bool hasMyXiaomiBracelet{false};
  // Bluetooth online upgrade
  bool hasBluetoothUpgrade{false};
  // healthy cool down
  bool hasLadderControl{false};
  // self-learning
  bool hasSelfLearn{false};
  // examination
  bool hasDeviceExamination{true};
  // filter to detect
  bool hasStrainerClean{false};
  // battery statistics
  bool hasPowerManager{false};
  bool hasStrong{false}; // 
  // comfort dehumidifier
  bool hasComfortDry{false};
  // manually pumping wet
  bool hasManualDry{false};
  //security monitoring
  bool hasSafeInvade{false};
  //intelligent control
  bool hasIntelControl{false};
  //gesture recognition
  bool hasGestureRecognize{false};
  //from clean control
  bool hasSelfCleaning{false};
  //voice function
  bool hasVoice{false};
  //hot and cold-sensitive features
  bool hasColdHot{false};
  //volume function
  bool hasVolume{true};
  bool hasYuyinVersion{false};
  //anti-straight blow
  bool hasPreventStraightLineWind{false};
  //child anti-cold wind
  bool hasChildrenPreventWind{false};
  //there are separate upper and lower no sense of style(not tucao translated copy of the above)
  bool hasUpDownNoWindFeel{false};
  bool isCentralAC{false};
  //display
  bool hasShow{true};
  //Keep Warm function
  bool hasKeepWarm{false};
  //the old Protocol to the anti-direct blowing feature
  bool hasWindBlowing{false};
  //filter dirty block
  bool hasFilterScreen{false};
  bool hasChangesTemperature{false};
  bool hasNoWindFeelModal{false};
  bool hasDisney{false};
  bool hasTime1TO1{false};
  bool hasVideoDescription{false};
};

#endif
}  // namespace midea_ac
}  // namespace esphome

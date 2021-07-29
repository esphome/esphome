#include "esphome/core/log.h"
#include "capabilities.h"
#include <pgmspace.h>
#include <string>

namespace esphome {
namespace midea_ac {

bool Capabilities::read(const Frame &frame) {
  const uint8_t *data = frame.data() + 10;
  const uint8_t length = frame.size() - 12;

  uint8_t i = 2;
  uint8_t caps2Process = data[1];

  while (i < length - 2 && caps2Process) {
    if (data[i + 1] == 0x00 && data[i + 2] > 0) {
      switch (data[i]) {
        case 0x15:
          this->indoor_humidity_ = data[i + 3] != 0;
          break;
        case 0x18:
          this->silky_cool_ = data[i + 3] != 0;
          break;
        case 0x30:
          this->smart_eye_ = data[i + 3] == 1;
          break;
        case 0x32:
          this->wind_on_me_ = data[i + 3] == 1;
          break;
        case 0x33: 
          this->wind_of_me_ = data[i + 3] == 1;
          break;
        case 0x39: 
          this->active_clean_ = data[i + 3] == 1;
          break;
        case 0x42:
          this->one_key_no_wind_on_me_ = data[i + 3] == 1;
          break;
        case 0x43:
          this->breeze_control_ = data[i + 3] == 1;
          break;
      }
    }

    if (data[i + 1] == 0x02 && data[i + 2] > 0) {
      switch (data[i]) {
        case 0x10:        
          this->fanspeed_control_ = data[i + 3] != 1;
          break;
        case 0x12:
          this->eco_mode_ = data[i + 3] == 1;
          this->special_eco_ = data[i + 3] == 2;
          break;
        case 0x13:
          this->frost_protection_mode_ = data[i + 3] == 1;
          break;
        case 0x14:
          switch (data[i + 3]) {
            case 0:
              this->heat_mode_ = false;
              this->cool_mode_ = true;
              this->dry_mode_ = true;
              this->auto_mode_ = true;
              break;
            case 1:
              this->cool_mode_ = true;
              this->heat_mode_= true;
              this->dry_mode_ = true;
              this->auto_mode_ = true;
              break;
            case 2:
              this->cool_mode_ = false;
              this->dry_mode_ = false;
              this->heat_mode_ = true;
              this->auto_mode_ = true;
              break;
            case 3:
              this->cool_mode_ = true;
              this->dry_mode_ = false;
              this->heat_mode_ = false;
              this->auto_mode_ = false;
              break;
          }
          break;
        case 0x15:
          switch (data[i + 3]) {
            case 0:
              this->leftright_fan_ = false;
              this->updown_fan_ = true;
              break;
            case 1:
              this->leftright_fan_ = true;
              this->updown_fan_ = true;
              break;
            case 2:
              this->leftright_fan_ = false;
              this->updown_fan_ = false;
              break;
            case 3:
              this->leftright_fan_ = true;
              this->updown_fan_ = false;
              break;
          }
          break;
        case 0x16: 
          switch (data[i + 3]) {
            case 0:
            case 1:
              this->power_cal_ = false;
              this->power_cal_setting_ = false;
              break;
            case 2:
              this->power_cal_ = true;
              this->power_cal_setting_ = false;
              break;
            case 3:
              this->power_cal_ = true;
              this->power_cal_setting_ = true;
              break;
          }
          break;
        case 0x17: 
          switch (data[i + 3]) {
            case 0:
              this->nest_check_ = false;
              this->nest_need_change_ = false;
              break;
            case 1: 
            case 2:
              this->nest_check_ = true;
              this->nest_need_change_ = false;
              break;
            case 3: 
              this->nest_check_ = false;
              this->nest_need_change_ = true;
              break;
            case 4: 
              this->nest_check_ = true;
              this->nest_need_change_ = true;
              break;
          }
          break;
        case 0x19:
          this->electric_aux_heating_ = data[i + 3] == 1;
          break;
        case 0x1A: 
          switch (data[i + 3]) {
            case 0:
              this->turbo_heat_ = false;
              this->turbo_cool_ = true;
              break;
            case 1:
              this->turbo_heat_ = true;
              this->turbo_cool_ = true;
              break;
            case 2:
              this->turbo_heat_ = false;
              this->turbo_cool_ = false;
              break;
            case 3:
              this->turbo_heat_ = true;
              this->turbo_cool_ = false;
              break;
          }
          break;
        case 0x1F:
          switch (data[i + 3]) {
            case 0: 
              this->auto_set_humidity_ = false;
              this->manual_set_humidity_ = false;
              break;
            case 1:
              this->auto_set_humidity_ = true;
              this->manual_set_humidity_ = false;
              break;
            case 2:
              this->auto_set_humidity_ = true;
              this->manual_set_humidity_ = true;
              break;
            case 3:
              this->auto_set_humidity_ = false;
              this->manual_set_humidity_ = true;
              break;
          }
          break;
        case 0x22:
          this->unit_changeable_ = data[i + 3] == 0;
          break;
        case 0x24:
          this->light_control_ = data[i + 3];
          break;
        case 0x25:
          if (data[i + 2] >= 6) {
            this->min_temp_cool_ = static_cast<float>(data[i + 3]) * 0.5f;
            this->max_temp_cool_ = static_cast<float>(data[i + 4]) * 0.5f;
            this->min_temp_auto_ = static_cast<float>(data[i + 5]) * 0.5f;
            this->max_temp_auto_ = static_cast<float>(data[i + 6]) * 0.5f;
            this->min_temp_heat_ = static_cast<float>(data[i + 7]) * 0.5f;
            this->max_temp_heat_ = static_cast<float>(data[i + 8]) * 0.5f;

            if (data[i + 2] > 6)
              this->decimals_ = data[i + 9] > 0;
            else
              this->decimals_ = data[i + 5] != 0;
          break;
        case 0x2C:
          this->buzzer_ = data[i + 3] != 0;
          break;
        }
      }
    }
    // Increment cursor and decrement capabilities to process
    i += (3 + data[i + 2]);
    caps2Process--;
  }
  
  if (length - i > 1)
    return data[length - 2] > 0;
  
  this->is_ready_ = true;
  return false;
}

const std::string Capabilities::FREEZE_PROTECTION = "freeze protection";
const std::string Capabilities::SILENT = "silent";
const std::string Capabilities::TURBO = "turbo";

void Capabilities::to_climate_traits(ClimateTraits &traits) const {
  /* TEMPERATURES */
  traits.set_supports_current_temperature(true);
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(0.5);

  /* MODES */
  traits.add_supported_mode(ClimateMode::CLIMATE_MODE_FAN_ONLY);
  if (this->auto_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT_COOL);
  if (this->cool_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_COOL);
  if (this->heat_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT);
  if (this->dry_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_DRY);

  /* FAN MODES */
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_HIGH);

  /* SWING MODES */
  traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_OFF);
  traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_VERTICAL);

  /* PRESETS */
  traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);
  traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_SLEEP);
  if (this->turbo_cool() || this->turbo_heat())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_BOOST);
  if (this->eco_mode())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_ECO);

  /* CUSTOM PRESETS */
  if (this->frost_protection_mode())
    traits.add_supported_custom_preset(FREEZE_PROTECTION);
}

#define LOG_CAPABILITY(tag, str, condition) \
  if (condition)                            \
    ESP_LOGCONFIG(tag, str);

void Capabilities::dump(const char *tag) const {
  ESP_LOGCONFIG(tag, "MIDEA CAPABILITIES REPORT:");
  if (this->auto_mode()) {
    ESP_LOGCONFIG(tag, "  [x] AUTO MODE");
    ESP_LOGCONFIG(tag, "      - MIN TEMP: %.1f", this->min_temp_auto());
    ESP_LOGCONFIG(tag, "      - MAX TEMP: %.1f", this->max_temp_auto());
  }
  if (this->cool_mode()) {
    ESP_LOGCONFIG(tag, "  [x] COOL MODE");
    ESP_LOGCONFIG(tag, "      - MIN TEMP: %.1f", this->min_temp_cool());
    ESP_LOGCONFIG(tag, "      - MAX TEMP: %.1f", this->max_temp_cool());
  }
  if (this->heat_mode()) {
    ESP_LOGCONFIG(tag, "  [x] HEAT MODE");
    ESP_LOGCONFIG(tag, "      - MIN TEMP: %.1f", this->min_temp_heat());
    ESP_LOGCONFIG(tag, "      - MAX TEMP: %.1f", this->max_temp_heat());
  }
  LOG_CAPABILITY(tag, "  [x] DRY MODE", this->dry_mode());
  LOG_CAPABILITY(tag, "  [x] ECO MODE", this->eco_mode());
  LOG_CAPABILITY(tag, "  [x] FROST PROTECTION MODE", this->frost_protection_mode());
  LOG_CAPABILITY(tag, "  [x] TURBO COOL", this->turbo_cool());
  LOG_CAPABILITY(tag, "  [x] TURBO HEAT", this->turbo_heat());
  LOG_CAPABILITY(tag, "  [x] FANSPEED CONTROL", this->fanspeed_control());
  LOG_CAPABILITY(tag, "  [x] BREEZE CONTROL", this->breeze_control());
  LOG_CAPABILITY(tag, "  [x] LIGHT CONTROL", this->light_control());
  LOG_CAPABILITY(tag, "  [x] UPDOWN FAN", this->updown_fan());
  LOG_CAPABILITY(tag, "  [x] LEFTRIGHT FAN", this->leftright_fan());
  LOG_CAPABILITY(tag, "  [x] AUTO SET HUMIDITY", this->auto_set_humidity());
  LOG_CAPABILITY(tag, "  [x] MANUAL SET HUMIDITY", this->manual_set_humidity());
  LOG_CAPABILITY(tag, "  [x] INDOOR HUMIDITY", this->indoor_humidity());
  LOG_CAPABILITY(tag, "  [x] POWER CAL", this->power_cal());
  LOG_CAPABILITY(tag, "  [x] POWER CAL SETTING", this->power_cal_setting());
  LOG_CAPABILITY(tag, "  [x] BUZZER", this->buzzer());
  LOG_CAPABILITY(tag, "  [x] SOUND", this->sound());
  LOG_CAPABILITY(tag, "  [x] ACTIVE CLEAN", this->active_clean());
  LOG_CAPABILITY(tag, "  [x] DECIMALS", this->decimals());
  LOG_CAPABILITY(tag, "  [x] ELECTRIC AUX HEATING", this->electric_aux_heating());
  LOG_CAPABILITY(tag, "  [x] NEST CHECK", this->nest_check());
  LOG_CAPABILITY(tag, "  [x] NEST NEED CHANGE", this->nest_need_change());
  LOG_CAPABILITY(tag, "  [x] ONE KEY NO WIND ON ME", this->one_key_no_wind_on_me());
  LOG_CAPABILITY(tag, "  [x] SILKY COOL", this->silky_cool());
  LOG_CAPABILITY(tag, "  [x] SMART EYE", this->smart_eye());
  LOG_CAPABILITY(tag, "  [x] SPECIAL ECO", this->special_eco());
  LOG_CAPABILITY(tag, "  [x] UNIT CHANGEABLE", this->unit_changeable());
  LOG_CAPABILITY(tag, "  [x] WIND OF ME", this->wind_of_me());
  LOG_CAPABILITY(tag, "  [x] WIND ON ME", this->wind_on_me());
}

#define DEVICE_LIST(name, data) static const char LIST_##name[] PROGMEM = data
#define DEVICE_NAME(name) static const char NAME_##name[] PROGMEM = #name
#define DEVICE_FUNC(name) static void FUNC_##name(DeviceInfo &di)
#define DEVICE_RECORD(name) { LIST_##name, NAME_##name, FUNC_##name }

struct DeviceDbRecord {
  const char *list;
  const char *name;
  std::function<void(DeviceInfo &)> func;
};

DEVICE_LIST(QA100, "11005/10977/11007/10979/10563/10567/10711/11415/11417/11423/11425/11467/11469/11471/11473/11475/11477");
DEVICE_LIST(SA100, "11175/11177/11419/11421/11437/11439/11441/Z1140");
DEVICE_LIST(SA200, "11285/11287/11351/11353/11355/11357");
DEVICE_LIST(SA300, "11179/11181/11183/11313/11315/11317/11537/11539/11541/Z1139/Z1149/Z2155/Z1166/Z2185");
DEVICE_LIST(YA, "11381/11387/11383/11385/11389/11393/11391/11395/11463/11465/11571/11573/11575/Z2165/Z2168/Z1151/Z1152/Z1153/Z1154/Z1158/Z2165/Z2166/Z2167/Z1167/Z2169/Z2170/Z2172/Z2186/11839/11919/11917/11915/11913");
DEVICE_LIST(26YA, "11835");
DEVICE_LIST(YAB3, "11397/11399/11401/11403/11405/11407/Z2170/Z1155/Z1156");
DEVICE_LIST(WJABC, "11295/11301/11297/11303/11299/11305");
DEVICE_LIST(YA100, "50491/50493/50271/50273/50603/50607/50663/50665");
DEVICE_LIST(CJ200, "50601/50509/50507/50605/Z1138");
DEVICE_LIST(WYA, "50585/50583");
DEVICE_LIST(WPA, "50559/50557");
DEVICE_LIST(LBB2, "50095/50163/50077/50167");
DEVICE_LIST(LBB3, "50119/50213/50397/50081/50285/50403/50661/50695");
DEVICE_LIST(PA400, "50387/50401");
DEVICE_LIST(WEA, "11373/11375/11377/11379/11309/11311/10983/10985/F0283/F0285/F0287/F0289/F0291/F0293/F0295/F0297/F0299/F0301/F0303/F0305/Z1172/Z1211/Z1212/Z1173");
DEVICE_LIST(CA, "11447/11451/11453/11455/11457/11459/11525/11527");
DEVICE_LIST(TAB3, "11489/11491/11493/11505/11507/11509/Z1161/Z1162/Z2182/Z2183/11551/11553/11557/11561/11565");
DEVICE_LIST(TAB12, "11495/11497/11499/11501/11503/11511/11513/11515/Z1165/Z1164/Z1163/Z2180/Z2181/Z2184/11543/11545/11547/11549/11555/11559/11563/11585/11587/11833/11837/11931/11929/Z2240/Z1217/Z1219/Z2242/11945/11947");
DEVICE_LIST(KHB1, "50637/50639/50655/50653/50723");
DEVICE_LIST(CACP, "11533/11535");
DEVICE_LIST(ZA300, "50199/50201/50265/50269/50307/50373/50317/50375/50431/50433");
DEVICE_LIST(ZA300B3, "50251/50253/50281/50289/50309/50315/50383/50385/50427/50429/50669/50701");
DEVICE_LIST(ZB300, "50657/50659");
DEVICE_LIST(ZB300YJ, "Z1170/Z2191");
DEVICE_LIST(YA300, "50205/50207/50393/50451");
DEVICE_LIST(YA301, "50531/50533/50677/50687/G0041/G0043/G0047/G0049/G0045/G0051/50777/50779/Z2231/Z1207");
DEVICE_LIST(YA302, "50577/50579/50679/50693");
DEVICE_LIST(YA201, "50609/50681/50689/50611/50675/50685/50775/50773");
DEVICE_LIST(YA200, "50771/50769");
DEVICE_LIST(PC400B23, "11367/11369/11371/11323/11327/11445/11449/11461/11757/11759/11761/11323/11325/11327/11445/11449/11461/F0473/Z2217");
DEVICE_LIST(WXA, "11695/11697/Z1209/Z2234/12023/12025");
DEVICE_LIST(WJ7, "10167/10169/10171");
DEVICE_LIST(WYB, "50725/50727");
DEVICE_LIST(PCB50, "11719");
DEVICE_LIST(YB200, "50731/50733/Z1184/Z2208");
DEVICE_LIST(YB200TEST, "55555");
DEVICE_LIST(DA100, "11717/11711");
DEVICE_LIST(WXD, "11713/11715/Z1208/Z2233");
DEVICE_LIST(YB300, "50753/50755/Z1187/Z2212/50841/50839/50835/50837/Z1204/Z2229/50903/Z2254/Z1231/50905/Z1231/Z2254");
DEVICE_LIST(WDAA3, "11763/11765/Z2214/11879/11885/Z1210/Z2235/11991/11989/11993/11995/12019/12021");
DEVICE_LIST(WDAD3, "11735/11737/11883/11881/11799/11803/11801/11805");
DEVICE_LIST(WYAD2D3, "50763/50599/50761/50589/50597/Z1216/Z2239");
DEVICE_LIST(WPAD3, "50749/50751/50737/50739/50741/50743");
DEVICE_LIST(DA400B3, "11605/11613/11641/11729/11731/11733/Z3063/Z3065/Z2187/Z2215/11843/11231");
DEVICE_LIST(PC400B1, "11775/11777/11829/11831/11937/11933");
DEVICE_LIST(DA200300, "11589/11591/11593/11597/11599/11649/Z1168/Z2188/Z2164/11739/11741/11743/11745/11747/11749/11661/11663/11667/Z1169/Z2189/11751/11753/11755/11821/11823/11825/11827/Z1191/Z2218/Z1192/Z2219/11891/11889/11925/11927/Z1206/11941/11943/12005/12007/12009/12013/12011/12013/12011");
DEVICE_LIST(DA400D3, "11779/11781/11783/11785/11787/11789/11791/11793");
DEVICE_LIST(DA400BP, "50853/50847/Z2241/Z1218");
DEVICE_LIST(DA400DP, "50843/50845/50849/50851");
DEVICE_LIST(PA400B3, "50745/50747");
DEVICE_LIST(J9, "19003/19001/Z2900/Z1900/Z1902/19013/19014/19011/19012/Z2902/19017/19015");
DEVICE_LIST(YA400D2D3, "50569/50571/50587/50593/50757/50759/50647/50648/50649");
DEVICE_LIST(QA301B1, "11795/11797/11796/11798/Z2213/Z1188");
DEVICE_LIST(WCBA3, "F0275/F0277/F0279/F0281/F0307/F0309/F0311/F0313/F0315/F0317/11701/11699/11987/11985");
DEVICE_LIST(IQ100, "Z1194/11819/11813/Z2221/Z1198");
DEVICE_LIST(DA100Z, "11809/11811/Z1193/Z2220/11907/11905/11911/11909/11895/11893/11923/11921/Z1201/Z2232");
DEVICE_LIST(IQ300, "Z1195/Z2222/11815/11817");
DEVICE_LIST(J7, "Z2901/Z1008/Z1901/50103/50101/50079/50081/50781/50783/50109/50107/Z1205/Z2230");
DEVICE_LIST(YA400B2, "50565/50567/50613/50621/50683/50691");
DEVICE_LIST(WPBC, "50789/50791/50785/50787/Z2237/Z1214");
DEVICE_LIST(WPCD, "50797/50799");
DEVICE_LIST(YB400, "50823/50825/Z2227/Z1199/50891/50893/50907/50909/50947/50945/50949/50951");
DEVICE_LIST(PE400B3, "11723/11727");
DEVICE_LIST(J8, "Z2903/Z1903/50113/50111");
DEVICE_LIST(YB301, "50861/50863/Z1215/Z2238/Z1223/Z2246/50875/50873/50877/50879/Z1224/Z2247/50917/50919/Z1241/50921/50923/Z1242/50933/50935/50941/50943/Z1264");
DEVICE_LIST(WYS, "50895/50897/Z1244");
DEVICE_LIST(1TO1, "96901/96902/96903/96904/96135");
DEVICE_LIST(YB201, "50869/50871/Z1226/Z2249/Z1227/Z2250/50889/50887/50925/50927/Z1243");
DEVICE_LIST(PF200, "11963/11961/Z2245/Z1222/11981/11983/Z1234/Z2258");
DEVICE_LIST(GM100, "11967/11965/Z1229/Z2252");
DEVICE_LIST(WOW, "11979/11977/Z1235/Z2259/Z2266");
DEVICE_LIST(1TON, "PD004");
DEVICE_LIST(S10, "20003/20001/Z1904/Z2904");
DEVICE_LIST(FA100, "12037/12035/Z1261/Z1262");
DEVICE_LIST(FA200, "12039/12041/Z1263");
DEVICE_LIST(W10, "60001/60003");
DEVICE_LIST(WXDF, "12070/12072");
DEVICE_LIST(YB100, "50939/Z1259");
DEVICE_LIST(MQ200, "Z2270/Z2269/Z1249/Z1248/12065/12067");
DEVICE_LIST(GW10, "Z1908/20017");

DEVICE_NAME(QA100);
DEVICE_NAME(SA100);
DEVICE_NAME(SA200);
DEVICE_NAME(SA300);
DEVICE_NAME(YA);
DEVICE_NAME(26YA);
DEVICE_NAME(YAB3);
DEVICE_NAME(WJABC);
DEVICE_NAME(YA100);
DEVICE_NAME(CJ200);
DEVICE_NAME(WYA);
DEVICE_NAME(WPA);
DEVICE_NAME(LBB2);
DEVICE_NAME(LBB3);
DEVICE_NAME(PA400);
DEVICE_NAME(WEA);
DEVICE_NAME(CA);
DEVICE_NAME(TAB3);
DEVICE_NAME(TAB12);
DEVICE_NAME(KHB1);
DEVICE_NAME(CACP);
DEVICE_NAME(ZA300);
DEVICE_NAME(ZA300B3);
DEVICE_NAME(ZB300);
DEVICE_NAME(ZB300YJ);
DEVICE_NAME(YA300);
DEVICE_NAME(YA201);
DEVICE_NAME(YA200);
DEVICE_NAME(YA301);
DEVICE_NAME(YA302);
DEVICE_NAME(PC400B23);
DEVICE_NAME(WXA);
DEVICE_NAME(WJ7);
DEVICE_NAME(WYB);
DEVICE_NAME(PCB50);
DEVICE_NAME(YB200);
DEVICE_NAME(DA100);
DEVICE_NAME(YB200TEST);
DEVICE_NAME(WXD);
DEVICE_NAME(YB300);
DEVICE_NAME(WDAA3);
DEVICE_NAME(WDAD3);
DEVICE_NAME(WYAD2D3);
DEVICE_NAME(WPAD3);
DEVICE_NAME(DA400B3);
DEVICE_NAME(PC400B1);
DEVICE_NAME(DA200300);
DEVICE_NAME(DA400D3);
DEVICE_NAME(DA400BP);
DEVICE_NAME(DA400DP);
DEVICE_NAME(PA400B3);
DEVICE_NAME(J9);
DEVICE_NAME(YA400D2D3);
DEVICE_NAME(QA301B1);
DEVICE_NAME(WCBA3);
DEVICE_NAME(IQ100);
DEVICE_NAME(DA100Z);
DEVICE_NAME(IQ300);
DEVICE_NAME(J7);
DEVICE_NAME(WPBC);
DEVICE_NAME(WPCD);
DEVICE_NAME(YB400);
DEVICE_NAME(PE400B3);
DEVICE_NAME(J8);
DEVICE_NAME(YA400B2);
DEVICE_NAME(YB301);
DEVICE_NAME(WYS);
DEVICE_NAME(1TO1);
DEVICE_NAME(1TON);
DEVICE_NAME(YB201);
DEVICE_NAME(PF200);
DEVICE_NAME(GM100);
DEVICE_NAME(WOW);
DEVICE_NAME(S10);
DEVICE_NAME(FA100);
DEVICE_NAME(FA200);
DEVICE_NAME(W10);
DEVICE_NAME(WXDF);
DEVICE_NAME(YB100);
DEVICE_NAME(MQ200);
DEVICE_NAME(GW10);
DEVICE_NAME(OTHER);

DEVICE_FUNC(QA100) {
  di.deviceType = DeviceID::ID_QA100;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(SA100) {
  di.deviceType = DeviceID::ID_SA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasHumidityDisplay = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(SA200) {
  di.deviceType = DeviceID::ID_SA200;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(SA300) {
  di.deviceType = DeviceID::ID_SA300;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YA) {
  di.deviceType = DeviceID::ID_YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WOW) {
  di.deviceType = DeviceID::ID_WOW;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasFilterScreen = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(26YA) {
  di.deviceType = DeviceID::ID_26YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(GM100) {
  di.deviceType = DeviceID::ID_GM100;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(S10) {
  di.deviceType = DeviceID::ID_S10;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(YAB3) {
  di.deviceType = DeviceID::ID_YAB3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(WJABC) {
  di.deviceType = DeviceID::ID_WJABC;
  di.hasUpDownSwipeWind = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YA100) {
  di.deviceType = DeviceID::ID_YA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
}

DEVICE_FUNC(CJ200) {
  di.deviceType = DeviceID::ID_CJ200;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(WYA) {
  di.deviceType = DeviceID::ID_WYA;
  di.hasSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(WPA) {
  di.deviceType = DeviceID::ID_WPA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(LBB2) {
  di.deviceType = DeviceID::ID_LBB2;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(LBB3) {
  di.deviceType = DeviceID::ID_LBB3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(PA400) {
  di.deviceType = DeviceID::ID_PA400;
  di.hasUpDownSwipeWind = true;
}

DEVICE_FUNC(WEA) {
  di.deviceType = DeviceID::ID_WEA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(MQ200) {
  di.deviceType = DeviceID::ID_MQ200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasDisney = true;
}

DEVICE_FUNC(CA) {
  di.deviceType = DeviceID::ID_CA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasMyXiaomiBracelet = true;
  di.hasBluetoothUpgrade = true;
}

DEVICE_FUNC(TAB3) {
  di.deviceType = DeviceID::ID_TAB3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasNoWindFeel = true;
  di.hasNatureWind = true;
  di.hasPMV = true;
  di.hasDot5Support = true;
  di.hasHumidityDisplay = true;
  di.hasLadderControl = true;
  di.hasSleepCurve = true;
  di.hasComfortDry = true;
  di.hasManualDry = true;
}

DEVICE_FUNC(TAB12) {
  di.deviceType = DeviceID::ID_TAB12;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasNoWindFeel = true;
  di.hasNatureWind = true;
  di.hasPMV = true;
  di.hasDot5Support = true;
  di.hasHumidityDisplay = true;
  di.hasLadderControl = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
  di.hasComfortDry = true;
  di.hasManualDry = true;
}

DEVICE_FUNC(FA100) {
  di.deviceType = DeviceID::ID_FA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasHumidityDisplay = true;
  di.hasLadderControl = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
  di.hasComfortDry = true;
  di.hasManualDry = true;
  di.hasChangesTemperature = true;
  di.hasNoWindFeelModal = true;
  di.hasChildrenPreventCold = true;
}

DEVICE_FUNC(FA200) {
  di.deviceType = DeviceID::ID_FA200;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasHumidityDisplay = true;
  di.hasLadderControl = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
  di.hasComfortDry = true;
  di.hasManualDry = true;
  di.hasNoWindFeelModal = true;
}

DEVICE_FUNC(KHB1) {
  di.deviceType = DeviceID::ID_KHB1;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
  di.hasReadyColdOrHot = true;
  di.hasPMV = true;
  di.hasDot5Support = true;
  di.hasHumidityDisplay = true;
  di.hasComfortDry = true;
  di.hasManualDry = true;
}

DEVICE_FUNC(J8) {
  di.deviceType = DeviceID::ID_J8;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(CACP) {
  di.deviceType = DeviceID::ID_CACP;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(ZA300) {
  di.deviceType = DeviceID::ID_ZA300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(ZA300B3) {
  di.deviceType = DeviceID::ID_ZA300B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(ZB300) {
  di.deviceType = DeviceID::ID_ZB300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(ZB300YJ) {
  di.deviceType = DeviceID::ID_ZB300YJ;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(YA300) {
  di.deviceType = DeviceID::ID_YA300;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(YA301) {
  di.deviceType = DeviceID::ID_YA301;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(W10) {
  di.deviceType = DeviceID::ID_W10;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
}

DEVICE_FUNC(YA201) {
  di.deviceType = DeviceID::ID_YA201;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(YA200) {
  di.deviceType = DeviceID::ID_YA200;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(YA302) {
  di.deviceType = DeviceID::ID_YA302;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(PC400B23) {
  di.deviceType = DeviceID::ID_PC400B23;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(WXA) {
  di.deviceType = DeviceID::ID_WXA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasPowerManager = true;
  di.hasReadyColdOrHot = true;
  di.hasStrainerClean = true;
  di.hasLadderControl = true;
}

DEVICE_FUNC(WJ7) {
  di.deviceType = DeviceID::ID_WJ7;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WYB) {
  di.deviceType = DeviceID::ID_WYB;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(PCB50) {
  di.deviceType = DeviceID::ID_PCB50;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(YB200) {
  di.deviceType = DeviceID::ID_YB200;
  di.hasComfortDry = true;
  di.hasManualDry = true;
  di.hasPMV = true;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
  di.hasDot5Support = true;
  di.hasLadderControl = true;
  di.hasHumidityDisplay = true;
  di.hasSafeInvade = true;
  di.hasIntelControl = true;
  di.hasGestureRecognize = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(YB100) {
  di.deviceType = DeviceID::ID_YB100;
  di.hasUpDownSwipeWind = true;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasPurify = true;
  di.hasDot5Support = true;
  di.hasLadderControl = true;
  di.hasSafeInvade = true;
  di.hasIntelControl = true;
  di.hasGestureRecognize = true;
  di.hasReadyColdOrHot = true;
  di.hasUpDownNoWindFeel = true;
  di.hasLadderControl = true;
}

DEVICE_FUNC(DA100) {
  di.deviceType = DeviceID::ID_DA100;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(DA100Z) {
  di.deviceType = DeviceID::ID_DA100Z;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YB200TEST) {
  di.deviceType = DeviceID::ID_YB200TEST;
  di.hasComfortDry = true;
  di.hasManualDry = true;
  di.hasPMV = true;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
  di.hasDot5Support = true;
  di.hasLadderControl = true;
  di.hasHumidityDisplay = true;
  di.hasSafeInvade = true;
  di.hasIntelControl = true;
  di.hasGestureRecognize = true;
  di.hasReadyColdOrHot = true;
  di.hasStrainerClean = true;
  di.hasPowerManager = true;
}

DEVICE_FUNC(WXD) {
  di.deviceType = DeviceID::ID_WXD;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WXDF) {
  di.deviceType = DeviceID::ID_WXDF;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YB300) {
  di.deviceType = DeviceID::ID_YB300;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasLadderControl = true;
}

DEVICE_FUNC(WDAA3) {
  di.deviceType = DeviceID::ID_WDAA3;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasECO = true;
}

DEVICE_FUNC(WDAD3) {
  di.deviceType = DeviceID::ID_WDAD3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WYAD2D3) {
  di.deviceType = DeviceID::ID_WYAD2D3;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

DEVICE_FUNC(WPAD3) {
  di.deviceType = DeviceID::ID_WPAD3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
  di.hasNoPolar = false;
}

DEVICE_FUNC(DA400B3) {
  di.deviceType = DeviceID::ID_DA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(PC400B1) {
  di.deviceType = DeviceID::ID_PC400B1;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(GW10) {
  di.deviceType = DeviceID::ID_GW10;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(PE400B3) {
  di.deviceType = DeviceID::ID_PE400B3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(DA200300) {
  di.deviceType = DeviceID::ID_DA200300;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasECO = true;
}

DEVICE_FUNC(DA400D3) {
  di.deviceType = DeviceID::ID_DA400D3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(DA400BP) {
  di.deviceType = DeviceID::ID_DA400BP;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDot5Support = true;
  di.hasPreventStraightLineWind = true;
}

DEVICE_FUNC(DA400DP) {
  di.deviceType = DeviceID::ID_DA400DP;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasOuterDoorDisplay = false;
  di.hasPreventStraightLineWind = true;
}

DEVICE_FUNC(PA400B3) {
  di.deviceType = DeviceID::ID_PA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(J9) {
  di.deviceType = DeviceID::ID_J9;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(YA400D2D3) {
  di.deviceType = DeviceID::ID_YA400D2D3;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

DEVICE_FUNC(QA301B1) {
  di.deviceType = DeviceID::ID_QA301B1;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(WCBA3) {
  di.deviceType = DeviceID::ID_WCBA3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(IQ100) {
  di.deviceType = DeviceID::ID_IQ100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasHumidityDisplay = true;
  di.hasLadderControl = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
  di.hasComfortDry = true;
  di.hasManualDry = true;
  di.hasVoice = true;
  di.hasColdHot = true;
  di.hasYuyinVersion = true;
}

DEVICE_FUNC(IQ300) {
  di.deviceType = DeviceID::ID_IQ300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasHumidityDisplay = true;
  di.hasLadderControl = true;
  di.hasSleepCurve = true;
  di.hasComfortDry = true;
  di.hasManualDry = true;
  di.hasVoice = true;
  di.hasYuyinVersion = true;
}

DEVICE_FUNC(J7) {
  di.deviceType = DeviceID::ID_J7;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(YA400B2) {
  di.deviceType = DeviceID::ID_YA400B2;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(WPBC) {
  di.deviceType = DeviceID::ID_WPBC;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(WPCD) {
  di.deviceType = DeviceID::ID_WPCD;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

DEVICE_FUNC(YB400) {
  di.deviceType = DeviceID::ID_YB400;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(YB301) {
  di.deviceType = DeviceID::ID_YB301;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
}

DEVICE_FUNC(WYS) {
  di.deviceType = DeviceID::ID_WYS;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasUpDownNoWindFeel = true;
}

DEVICE_FUNC(YB201) {
  di.deviceType = DeviceID::ID_YB201;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
  di.hasLadderControl = true;
}

DEVICE_FUNC(1TO1) {
  di.deviceType = DeviceID::ID_1TO1;
  di.hasElectricHeat = true;
  di.hasDry = true;
  di.hasShow = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasTime1TO1 = true;
}

DEVICE_FUNC(PF200) {
  di.deviceType = DeviceID::ID_PF200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(1TON) {
  di.deviceType = DeviceID::ID_1TON;
  di.isCentralAC = true;
}

static const DeviceDbRecord DEVICES_DBASE[] PROGMEM = {
  DEVICE_RECORD(QA100),     // 0
  DEVICE_RECORD(SA100),     // 1
  DEVICE_RECORD(SA200),     // 2
  DEVICE_RECORD(SA300),     // 3
  DEVICE_RECORD(YA),        // 4
  DEVICE_RECORD(26YA),      // 5
  DEVICE_RECORD(YAB3),      // 6
  DEVICE_RECORD(WJABC),     // 7
  DEVICE_RECORD(YA100),     // 8
  DEVICE_RECORD(CJ200),     // 9
  DEVICE_RECORD(WYA),       // 10
  DEVICE_RECORD(WPA),       // 11
  DEVICE_RECORD(LBB2),      // 12
  DEVICE_RECORD(LBB3),      // 13
  DEVICE_RECORD(PA400),     // 14
  DEVICE_RECORD(WEA),       // 15
  DEVICE_RECORD(CA),        // 16
  DEVICE_RECORD(TAB3),      // 17
  DEVICE_RECORD(TAB12),     // 18
  DEVICE_RECORD(KHB1),      // 19
  DEVICE_RECORD(CACP),      // 20
  DEVICE_RECORD(ZA300),     // 21
  DEVICE_RECORD(ZA300B3),   // 22
  DEVICE_RECORD(ZB300),     // 23
  DEVICE_RECORD(ZB300YJ),   // 24
  DEVICE_RECORD(YA300),     // 25
  DEVICE_RECORD(YA201),     // 26
  DEVICE_RECORD(YA200),     // 27
  DEVICE_RECORD(YA301),     // 28
  DEVICE_RECORD(YA302),     // 29
  DEVICE_RECORD(PC400B23),  // 30
  DEVICE_RECORD(WXA),       // 31
  DEVICE_RECORD(WJ7),       // 32
  DEVICE_RECORD(WYB),       // 33
  DEVICE_RECORD(PCB50),     // 34
  DEVICE_RECORD(YB200),     // 35
  DEVICE_RECORD(DA100),     // 36
  DEVICE_RECORD(YB200TEST), // 37
  DEVICE_RECORD(WXD),       // 38
  DEVICE_RECORD(YB300),     // 39
  DEVICE_RECORD(WDAA3),     // 40
  DEVICE_RECORD(WDAD3),     // 41
  DEVICE_RECORD(WYAD2D3),   // 42
  DEVICE_RECORD(WPAD3),     // 43
  DEVICE_RECORD(DA400B3),   // 44
  DEVICE_RECORD(PC400B1),   // 45
  DEVICE_RECORD(DA200300),  // 46
  DEVICE_RECORD(DA400D3),   // 47
  DEVICE_RECORD(DA400BP),   // 48
  DEVICE_RECORD(DA400DP),   // 49
  DEVICE_RECORD(PA400B3),   // 50
  DEVICE_RECORD(J9),        // 51
  DEVICE_RECORD(YA400D2D3), // 52
  DEVICE_RECORD(QA301B1),   // 53
  DEVICE_RECORD(WCBA3),     // 54
  DEVICE_RECORD(IQ100),     // 55
  DEVICE_RECORD(DA100Z),    // 56
  DEVICE_RECORD(IQ300),     // 57
  DEVICE_RECORD(J7),        // 58
  DEVICE_RECORD(WPBC),      // 59
  DEVICE_RECORD(WPCD),      // 60
  DEVICE_RECORD(YB400),     // 61
  DEVICE_RECORD(PE400B3),   // 62
  DEVICE_RECORD(J8),        // 63
  DEVICE_RECORD(YA400B2),   // 64
  DEVICE_RECORD(YB301),     // 65
  DEVICE_RECORD(WYS),       // 66
  DEVICE_RECORD(1TO1),      // 67
  DEVICE_RECORD(1TON),      // 68
  DEVICE_RECORD(YB201),     // 69
  DEVICE_RECORD(PF200),     // 70
  DEVICE_RECORD(GM100),     // 71
  DEVICE_RECORD(WOW),       // 72
  DEVICE_RECORD(S10),       // 73
  DEVICE_RECORD(FA100),     // 74
  DEVICE_RECORD(FA200),     // 75
  DEVICE_RECORD(W10),       // 76
  DEVICE_RECORD(WXDF),      // 77
  DEVICE_RECORD(YB100),     // 78
  DEVICE_RECORD(MQ200),     // 79
  DEVICE_RECORD(GW10),      // 80
};

static const uint8_t extraDeviceTypeForSelfClean[] PROGMEM = {
  ID_YA, ID_26YA, ID_YAB3, ID_LBB3, ID_WEA, ID_TAB3, ID_TAB12, ID_KHB1, ID_ZA300B3, ID_ZB300,
  ID_YA201, ID_YA200, ID_YA301, ID_YA302, ID_PC400B23, ID_WXA, ID_WJ7, ID_WYB, ID_PCB50, ID_YB200,
  ID_WXD, ID_YB300, ID_WDAA3, ID_DA400B3, ID_PC400B1, ID_DA200300, ID_DA400BP, ID_DA400DP, ID_J9, ID_QA301B1,
  ID_WCBA3, ID_DA100Z, ID_J7, ID_WPBC, ID_YB400, ID_PE400B3, ID_J8, ID_YA400B2, ID_YB301, ID_WYS,
  ID_YB201, ID_PF200, ID_GM100, ID_WOW, ID_S10, ID_FA100, ID_FA200, ID_W10, ID_WXDF, ID_YB100,
  ID_MQ200, ID_GW10
};

static const uint8_t extraDeviceTypeForKeepWarm[] PROGMEM = {
  ID_GM100, ID_WOW, ID_S10, ID_FA100, ID_FA200, ID_WXDF, ID_GW10
};

static const uint8_t extraDeviceTypeForYADot5[] PROGMEM = { ID_YA };
static const uint8_t extraDeviceTypeFor26YADot5[] PROGMEM = { ID_26YA };
static const uint8_t extraDeviceTypeForYADot5YAB3[] PROGMEM = { ID_YAB3 };

struct SnData {
  char barcode[6];
  uint8_t version;
  int8_t year;
  int8_t month;
  int8_t day;
};

static int8_t hex2int(char hex) {
  if (hex >= '0' && hex <= '9')
    return hex - '0';
  if (hex >= 'A' && hex <= 'F')
    return hex - 'A' + 10;
  if (hex >= 'a' && hex <= 'f')
    return hex - 'a' + 10;
  return -1;
}

static int8_t char2year(char ch) {
  const char years[] = "5678901234ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  auto ptr = strchr(years, ch);
  if (ptr == nullptr)
    return -1;
  return ptr - years + 15;
}

static SnData parseSN(const char *sn) {
  SnData ret;
  if (strlen(sn) == 32)
    sn += 6;
  strlcpy(ret.barcode, sn + 6, sizeof(ret.barcode));
  ret.version = hex2int(sn[0]);
  ret.year = char2year(sn[11]);
  ret.month = hex2int(sn[12]);
  ret.day = hex2int(sn[13]) * 10 + hex2int(sn[14]);
  return ret;
}

static bool check_date(const SnData &data, int8_t year, int8_t month, int8_t day) {
  if (data.year > year)
    return true;
  if (data.year < year)
    return false;
  if (data.month > month)
    return true;
  if (data.month < month)
    return false;
  return data.day >= day;
}

static bool check_id(uint8_t id, const uint8_t *pos, uint8_t num) {
  for (; num; --num, ++pos) {
    if (pgm_read_byte(pos) == id)
      return true;
  }
  return false;
}

#define CHECK_ID(pos, list, data, y, m, d) (check_id(pos, list, sizeof(list)) && check_date(data, y, m, d))

DeviceInfo DeviceInfo::fromSN(const char *sn) {
  DeviceInfo ret{};
  char buf[270];
  const SnData snData = parseSN(sn);
  const DeviceDbRecord *dbCursor = DEVICES_DBASE;
  for (uint8_t id = 0; id < sizeof(DEVICES_DBASE) / sizeof(DEVICES_DBASE[0]); ++id, ++dbCursor) {
    DeviceDbRecord dev;
    memcpy_P(&dev, dbCursor, sizeof(dev));
    strncpy_P(buf, dev.list, sizeof(buf));
    if (strstr(buf, snData.barcode) == nullptr)
      continue;
    if (snData.version != 13 && snData.version >= 2) {
      // YA: from 2017/09/06 or 26YA: from 2017/09/30
      if (CHECK_ID(id, extraDeviceTypeForYADot5, snData, 17, 9, 6) || CHECK_ID(id, extraDeviceTypeFor26YADot5, snData, 17, 9, 30)) {
        ret.hasDot5Support = true;
        ret.hasReadyColdOrHot = true;
        ret.hasKeepWarm = true;
      // YAB3: from 2017/09/06
      } else if (CHECK_ID(id, extraDeviceTypeForYADot5YAB3, snData, 17, 9, 6)) {
        ret.hasDot5Support = true;
        ret.hasReadyColdOrHot = true;
      // List of KeepWarm devices: from 2017/07/01
      } else if (CHECK_ID(id, extraDeviceTypeForKeepWarm, snData, 17, 7, 1)) {
        ret.hasKeepWarm = true;
      }
      // List of SelfClean devices: from 2016/09/01
      if (CHECK_ID(id, extraDeviceTypeForSelfClean, snData, 16, 9, 1)) {
        ret.hasSelfCleaning = true;
      }
    }
    dev.func(ret);
    strncpy_P(ret.id, dev.name, sizeof(ret.id));
    return ret;
  }
  ret.deviceType = DeviceID::ID_OTHER;
  strncpy_P(ret.id, NAME_OTHER, sizeof(ret.id));
  ret.hasUpDownSwipeWind = true;
  ret.hasLeftRightSwipeWind = true;
  return ret;
}

}  // namespace midea_ac
}  // namespace esphome

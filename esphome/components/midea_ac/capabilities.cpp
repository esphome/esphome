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
#define DEVICE_DATA(name) { LIST_##name, NAME_##name, FUNC_##name }

struct DeviceData {
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
  di.deviceType = DeviceType::QA100;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(SA100) {
  di.deviceType = DeviceType::SA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasHumidityDisplay = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(SA200) {
  di.deviceType = DeviceType::SA200;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(SA300) {
  di.deviceType = DeviceType::SA300;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YA) {
  di.deviceType = DeviceType::YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WOW) {
  di.deviceType = DeviceType::WOW;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasFilterScreen = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(26YA) {
  di.deviceType = DeviceType::YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(GM100) {
  di.deviceType = DeviceType::GM100;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(S10) {
  di.deviceType = DeviceType::GM100;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(YAB3) {
  di.deviceType = DeviceType::YAB3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(WJABC) {
  di.deviceType = DeviceType::WJABC;
  di.hasUpDownSwipeWind = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YA100) {
  di.deviceType = DeviceType::YA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
}

DEVICE_FUNC(CJ200) {
  di.deviceType = DeviceType::CJ200;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(WYA) {
  di.deviceType = DeviceType::WYA;
  di.hasSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(WPA) {
  di.deviceType = DeviceType::WPA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(LBB2) {
  di.deviceType = DeviceType::LBB2;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(LBB3) {
  di.deviceType = DeviceType::LBB3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(PA400) {
  di.deviceType = DeviceType::PA400;
  di.hasUpDownSwipeWind = true;
}

DEVICE_FUNC(WEA) {
  di.deviceType = DeviceType::WEA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(MQ200) {
  di.deviceType = DeviceType::MQ200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasDisney = true;
}

DEVICE_FUNC(CA) {
  di.deviceType = DeviceType::CA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasMyXiaomiBracelet = true;
  di.hasBluetoothUpgrade = true;
}

DEVICE_FUNC(TAB3) {
  di.deviceType = DeviceType::TAB3;
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
  di.deviceType = DeviceType::TAB12;
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
  di.deviceType = DeviceType::FA100;
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
  di.deviceType = DeviceType::FA200;
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
  di.deviceType = DeviceType::KHB1;
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
  di.deviceType = DeviceType::KHB1;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(CACP) {
  di.deviceType = DeviceType::CACP;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(ZA300) {
  di.deviceType = DeviceType::ZA300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(ZA300B3) {
  di.deviceType = DeviceType::ZA300B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(ZB300) {
  di.deviceType = DeviceType::ZB300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(ZB300YJ) {
  di.deviceType = DeviceType::ZB300YJ;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(YA300) {
  di.deviceType = DeviceType::YA300;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(YA301) {
  di.deviceType = DeviceType::YA301;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(W10) {
  di.deviceType = DeviceType::W10;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
}

DEVICE_FUNC(YA201) {
  di.deviceType = DeviceType::YA201;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(YA200) {
  di.deviceType = DeviceType::YA200;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(YA302) {
  di.deviceType = DeviceType::YA302;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

DEVICE_FUNC(PC400B23) {
  di.deviceType = DeviceType::PC400B23;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(WXA) {
  di.deviceType = DeviceType::WXA;
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
  di.deviceType = DeviceType::WJ7;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WYB) {
  di.deviceType = DeviceType::WYB;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(PCB50) {
  di.deviceType = DeviceType::PCB50;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(YB200) {
  di.deviceType = DeviceType::YB200;
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
  //di.deviceType = DeviceType::YB100;
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
  di.deviceType = DeviceType::DA100;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(DA100Z) {
  di.deviceType = DeviceType::DA100Z;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YB200TEST) {
  di.deviceType = DeviceType::YB200Test;
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
  di.deviceType = DeviceType::WXD;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WXDF) {
  //di.deviceType = DeviceType::WXDF;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(YB300) {
  di.deviceType = DeviceType::YB300;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasLadderControl = true;
}

DEVICE_FUNC(WDAA3) {
  di.deviceType = DeviceType::WDAA3;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasECO = true;
}

DEVICE_FUNC(WDAD3) {
  di.deviceType = DeviceType::WDAD3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(WYAD2D3) {
  di.deviceType = DeviceType::WYAD2D3;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

DEVICE_FUNC(WPAD3) {
  di.deviceType = DeviceType::WPAD3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
  di.hasNoPolar = false;
}

DEVICE_FUNC(DA400B3) {
  di.deviceType = DeviceType::DA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(PC400B1) {
  di.deviceType = DeviceType::PC400B1;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(GW10) {
  di.deviceType = DeviceType::GW10;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(PE400B3) {
  di.deviceType = DeviceType::PE400B3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(DA200300) {
  di.deviceType = DeviceType::DA200300;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasECO = true;
}

DEVICE_FUNC(DA400D3) {
  di.deviceType = DeviceType::DA400D3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

DEVICE_FUNC(DA400BP) {
  di.deviceType = DeviceType::DA400BP;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDot5Support = true;
  di.hasPreventStraightLineWind = true;
}

DEVICE_FUNC(DA400DP) {
  di.deviceType = DeviceType::DA400DP;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasOuterDoorDisplay = false;
  di.hasPreventStraightLineWind = true;
}

DEVICE_FUNC(PA400B3) {
  di.deviceType = DeviceType::PA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(J9) {
  //di.deviceType = DeviceType::WJ9;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

DEVICE_FUNC(YA400D2D3) {
  di.deviceType = DeviceType::YA400D2D3;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

DEVICE_FUNC(QA301B1) {
  di.deviceType = DeviceType::QA301B1;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(WCBA3) {
  di.deviceType = DeviceType::WCBA3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(IQ100) {
  di.deviceType = DeviceType::IQ100;
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
  di.deviceType = DeviceType::IQ300;
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
  di.deviceType = DeviceType::J7;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(YA400B2) {
  di.deviceType = DeviceType::YA400B2;
  di.hasLeftRightSwipeWind = true;
}

DEVICE_FUNC(WPBC) {
  di.deviceType = DeviceType::WPBC;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

DEVICE_FUNC(WPCD) {
  di.deviceType = DeviceType::WPCD;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

DEVICE_FUNC(YB400) {
  di.deviceType = DeviceType::YB400;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(YB301) {
  di.deviceType = DeviceType::YB301;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
}

DEVICE_FUNC(WYS) {
  di.deviceType = DeviceType::WYS;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasUpDownNoWindFeel = true;
}

DEVICE_FUNC(YB201) {
  di.deviceType = DeviceType::YB301;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
  di.hasLadderControl = true;
}

DEVICE_FUNC(1TO1) {
  di.deviceType = DeviceType::ONETOONE;
  di.hasElectricHeat = true;
  di.hasDry = true;
  di.hasShow = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasTime1TO1 = true;
}

DEVICE_FUNC(PF200) {
  di.deviceType = DeviceType::PF200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
}

DEVICE_FUNC(1TON) {
  di.deviceType = DeviceType::ONETOONE;
  di.isCentralAC = true;
}

static const DeviceData DEVICES_DATABASE[] PROGMEM = {
  DEVICE_DATA(QA100),     // 0
  DEVICE_DATA(SA100),     // 1
  DEVICE_DATA(SA200),     // 2
  DEVICE_DATA(SA300),     // 3
  DEVICE_DATA(YA),        // 4
  DEVICE_DATA(26YA),      // 5
  DEVICE_DATA(YAB3),      // 6
  DEVICE_DATA(WJABC),     // 7
  DEVICE_DATA(YA100),     // 8
  DEVICE_DATA(CJ200),     // 9
  DEVICE_DATA(WYA),       // 10
  DEVICE_DATA(WPA),       // 11
  DEVICE_DATA(LBB2),      // 12
  DEVICE_DATA(LBB3),      // 13
  DEVICE_DATA(PA400),     // 14
  DEVICE_DATA(WEA),       // 15
  DEVICE_DATA(CA),        // 16
  DEVICE_DATA(TAB3),      // 17
  DEVICE_DATA(TAB12),     // 18
  DEVICE_DATA(KHB1),      // 19
  DEVICE_DATA(CACP),      // 20
  DEVICE_DATA(ZA300),     // 21
  DEVICE_DATA(ZA300B3),   // 22
  DEVICE_DATA(ZB300),     // 23
  DEVICE_DATA(ZB300YJ),   // 24
  DEVICE_DATA(YA300),     // 25
  DEVICE_DATA(YA201),     // 26
  DEVICE_DATA(YA200),     // 27
  DEVICE_DATA(YA301),     // 28
  DEVICE_DATA(YA302),     // 29
  DEVICE_DATA(PC400B23),  // 30
  DEVICE_DATA(WXA),       // 31
  DEVICE_DATA(WJ7),       // 32
  DEVICE_DATA(WYB),       // 33
  DEVICE_DATA(PCB50),     // 34
  DEVICE_DATA(YB200),     // 35
  DEVICE_DATA(DA100),     // 36
  DEVICE_DATA(YB200TEST), // 37
  DEVICE_DATA(WXD),       // 38
  DEVICE_DATA(YB300),     // 39
  DEVICE_DATA(WDAA3),     // 40
  DEVICE_DATA(WDAD3),     // 41
  DEVICE_DATA(WYAD2D3),   // 42
  DEVICE_DATA(WPAD3),     // 43
  DEVICE_DATA(DA400B3),   // 44
  DEVICE_DATA(PC400B1),   // 45
  DEVICE_DATA(DA200300),  // 46
  DEVICE_DATA(DA400D3),   // 47
  DEVICE_DATA(DA400BP),   // 48
  DEVICE_DATA(DA400DP),   // 49
  DEVICE_DATA(PA400B3),   // 50
  DEVICE_DATA(J9),        // 51
  DEVICE_DATA(YA400D2D3), // 52
  DEVICE_DATA(QA301B1),   // 53
  DEVICE_DATA(WCBA3),     // 54
  DEVICE_DATA(IQ100),     // 55
  DEVICE_DATA(DA100Z),    // 56
  DEVICE_DATA(IQ300),     // 57
  DEVICE_DATA(J7),        // 58
  DEVICE_DATA(WPBC),      // 59
  DEVICE_DATA(WPCD),      // 60
  DEVICE_DATA(YB400),     // 61
  DEVICE_DATA(PE400B3),   // 62
  DEVICE_DATA(J8),        // 63
  DEVICE_DATA(YA400B2),   // 64
  DEVICE_DATA(YB301),     // 65
  DEVICE_DATA(WYS),       // 66
  DEVICE_DATA(1TO1),      // 67
  DEVICE_DATA(1TON),      // 68
  DEVICE_DATA(YB201),     // 69
  DEVICE_DATA(PF200),     // 70
  DEVICE_DATA(GM100),     // 71
  DEVICE_DATA(WOW),       // 72
  DEVICE_DATA(S10),       // 73
  DEVICE_DATA(FA100),     // 74
  DEVICE_DATA(FA200),     // 75
  DEVICE_DATA(W10),       // 76
  DEVICE_DATA(WXDF),      // 77
  DEVICE_DATA(YB100),     // 78
  DEVICE_DATA(MQ200),     // 79
  DEVICE_DATA(GW10),      // 80
};

static const uint8_t extraDeviceTypeForSelfClean[] PROGMEM = {
   4,  5,  6, 13, 15, 17, 18, 19, 22, 23,
  26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
  38, 39, 40, 44, 45, 46, 48, 49, 51, 53,
  54, 56, 58, 59, 61, 62, 63, 64, 65, 66,
  69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
  79, 80
};

static const uint8_t extraDeviceTypeForKeepWarm[] PROGMEM = {
  71, 72, 73, 74, 75, 77, 80
};

static const uint8_t extraDeviceTypeForYADot5[] PROGMEM = { 4 };
static const uint8_t extraDeviceTypeForYADot5jr26YA[] PROGMEM = { 5 };
static const uint8_t extraDeviceTypeForYADot5YAB3[] PROGMEM = { 6 };

void DeviceInfo::read(const std::string &sn) {
  const DeviceData *pgm = DEVICES_DATABASE;
  char buf[270];
  uint8_t pos = 0;
  for (; pos < sizeof(DEVICES_DATABASE) / sizeof(DEVICES_DATABASE[0]); ++pos, ++pgm) {
    DeviceData data;
    memcpy_P(&data, pgm, sizeof(data));
    strncpy_P(buf, data.list, sizeof(buf));
    if (strstr(buf, sn.c_str()) != nullptr) {
      data.func(*this);
      return;
    }
  }
  this->deviceType = DeviceType::OTHER;
  this->hasUpDownSwipeWind = true;
  this->hasLeftRightSwipeWind = true;
}

}  // namespace midea_ac
}  // namespace esphome

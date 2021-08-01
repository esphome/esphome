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
  uint8_t caps2_process = data[1];

  while (i < length - 2 && caps2_process) {
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
              this->heat_mode_ = true;
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
    caps2_process--;
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
  if (this->auto_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT_COOL);
  if (this->cool_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_COOL);
  if (this->heat_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_HEAT);
  if (this->dry_mode())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_DRY);
  if (this->turbo_cool() || this->turbo_heat())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_BOOST);
  if (this->eco_mode())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_ECO);
  if (this->frost_protection_mode())
    traits.add_supported_custom_preset(FREEZE_PROTECTION);
}

#define LOG_CAPABILITY(tag, str, condition) \
  if (condition) \
    ESP_LOGCONFIG(tag, str);

void Capabilities::dump(const char *tag) const {
  ESP_LOGCONFIG(tag, "CAPABILITIES REPORT:");
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
#if 0
#define AC_LIST(name, data) static const char LIST_##name[] PROGMEM = data
#define AC_NAME(name) static const char NAME_ID_##name[] PROGMEM = #name
#define AC_FUNC(name) static void FUNC_##name(DeviceInfo &di)
#define AC_RECORD(name) \
  { LIST_##name, NAME_##name, FUNC_##name }

struct DbRecord {
  const char *list;
  const char *name;
  std::function<void(DeviceInfo &)> func;
};

AC_LIST(ID_QA100, "11005/10977/11007/10979/10563/10567/10711/11415/11417/11423/11425/11467/11469/11471/11473/11475/11477");
AC_LIST(ID_SA100, "11175/11177/11419/11421/11437/11439/11441/Z1140");
AC_LIST(ID_SA200, "11285/11287/11351/11353/11355/11357");
AC_LIST(ID_SA300, "11179/11181/11183/11313/11315/11317/11537/11539/11541/Z1139/Z1149/Z2155/Z1166/Z2185");
AC_LIST(ID_YA, "11381/11387/11383/11385/11389/11393/11391/11395/11463/11465/11571/11573/11575/Z2165/Z2168/Z1151/Z1152/Z1153/Z1154/Z1158/Z2165/Z2166/Z2167/Z1167/Z2169/Z2170/Z2172/Z2186/11839/11919/11917/11915/11913");
AC_LIST(ID_26YA, "11835");
AC_LIST(ID_YAB3, "11397/11399/11401/11403/11405/11407/Z2170/Z1155/Z1156");
AC_LIST(ID_WJABC, "11295/11301/11297/11303/11299/11305");
AC_LIST(ID_YA100, "50491/50493/50271/50273/50603/50607/50663/50665");
AC_LIST(ID_CJ200, "50601/50509/50507/50605/Z1138");
AC_LIST(ID_WYA, "50585/50583");
AC_LIST(ID_WPA, "50559/50557");
AC_LIST(ID_LBB2, "50095/50163/50077/50167");
AC_LIST(ID_LBB3, "50119/50213/50397/50081/50285/50403/50661/50695");
AC_LIST(ID_PA400, "50387/50401");
AC_LIST(ID_WEA, "11373/11375/11377/11379/11309/11311/10983/10985/F0283/F0285/F0287/F0289/F0291/F0293/F0295/F0297/F0299/F0301/F0303/F0305/Z1172/Z1211/Z1212/Z1173");
AC_LIST(ID_CA, "11447/11451/11453/11455/11457/11459/11525/11527");
AC_LIST(ID_TAB3, "11489/11491/11493/11505/11507/11509/Z1161/Z1162/Z2182/Z2183/11551/11553/11557/11561/11565");
AC_LIST(ID_TAB12, "11495/11497/11499/11501/11503/11511/11513/11515/Z1165/Z1164/Z1163/Z2180/Z2181/Z2184/11543/11545/11547/11549/11555/11559/11563/11585/11587/11833/11837/11931/11929/Z2240/Z1217/Z1219/Z2242/11945/11947");
AC_LIST(ID_KHB1, "50637/50639/50655/50653/50723");
AC_LIST(ID_CACP, "11533/11535");
AC_LIST(ID_ZA300, "50199/50201/50265/50269/50307/50373/50317/50375/50431/50433");
AC_LIST(ID_ZA300B3, "50251/50253/50281/50289/50309/50315/50383/50385/50427/50429/50669/50701");
AC_LIST(ID_ZB300, "50657/50659");
AC_LIST(ID_ZB300YJ, "Z1170/Z2191");
AC_LIST(ID_YA300, "50205/50207/50393/50451");
AC_LIST(ID_YA301, "50531/50533/50677/50687/G0041/G0043/G0047/G0049/G0045/G0051/50777/50779/Z2231/Z1207");
AC_LIST(ID_YA302, "50577/50579/50679/50693");
AC_LIST(ID_YA201, "50609/50681/50689/50611/50675/50685/50775/50773");
AC_LIST(ID_YA200, "50771/50769");
AC_LIST(ID_PC400B23, "11367/11369/11371/11323/11327/11445/11449/11461/11757/11759/11761/11323/11325/11327/11445/11449/11461/F0473/Z2217");
AC_LIST(ID_WXA, "11695/11697/Z1209/Z2234/12023/12025");
AC_LIST(ID_WJ7, "10167/10169/10171");
AC_LIST(ID_WYB, "50725/50727");
AC_LIST(ID_PCB50, "11719");
AC_LIST(ID_YB200, "50731/50733/Z1184/Z2208");
AC_LIST(ID_YB200TEST, "55555");
AC_LIST(ID_DA100, "11717/11711");
AC_LIST(ID_WXD, "11713/11715/Z1208/Z2233");
AC_LIST(ID_YB300, "50753/50755/Z1187/Z2212/50841/50839/50835/50837/Z1204/Z2229/50903/Z2254/Z1231/50905/Z1231/Z2254");
AC_LIST(ID_WDAA3, "11763/11765/Z2214/11879/11885/Z1210/Z2235/11991/11989/11993/11995/12019/12021");
AC_LIST(ID_WDAD3, "11735/11737/11883/11881/11799/11803/11801/11805");
AC_LIST(ID_WYAD2D3, "50763/50599/50761/50589/50597/Z1216/Z2239");
AC_LIST(ID_WPAD3, "50749/50751/50737/50739/50741/50743");
AC_LIST(ID_DA400B3, "11605/11613/11641/11729/11731/11733/Z3063/Z3065/Z2187/Z2215/11843/11231");
AC_LIST(ID_PC400B1, "11775/11777/11829/11831/11937/11933");
AC_LIST(ID_DA200300, "11589/11591/11593/11597/11599/11649/Z1168/Z2188/Z2164/11739/11741/11743/11745/11747/11749/11661/11663/11667/Z1169/Z2189/11751/11753/11755/11821/11823/11825/11827/Z1191/Z2218/Z1192/Z2219/11891/11889/11925/11927/Z1206/11941/11943/12005/12007/12009/12013/12011/12013/12011");
AC_LIST(ID_DA400D3, "11779/11781/11783/11785/11787/11789/11791/11793");
AC_LIST(ID_DA400BP, "50853/50847/Z2241/Z1218");
AC_LIST(ID_DA400DP, "50843/50845/50849/50851");
AC_LIST(ID_PA400B3, "50745/50747");
AC_LIST(ID_J9, "19003/19001/Z2900/Z1900/Z1902/19013/19014/19011/19012/Z2902/19017/19015");
AC_LIST(ID_YA400D2D3, "50569/50571/50587/50593/50757/50759/50647/50648/50649");
AC_LIST(ID_QA301B1, "11795/11797/11796/11798/Z2213/Z1188");
AC_LIST(ID_WCBA3, "F0275/F0277/F0279/F0281/F0307/F0309/F0311/F0313/F0315/F0317/11701/11699/11987/11985");
AC_LIST(ID_IQ100, "Z1194/11819/11813/Z2221/Z1198");
AC_LIST(ID_DA100Z, "11809/11811/Z1193/Z2220/11907/11905/11911/11909/11895/11893/11923/11921/Z1201/Z2232");
AC_LIST(ID_IQ300, "Z1195/Z2222/11815/11817");
AC_LIST(ID_J7, "Z2901/Z1008/Z1901/50103/50101/50079/50081/50781/50783/50109/50107/Z1205/Z2230");
AC_LIST(ID_YA400B2, "50565/50567/50613/50621/50683/50691");
AC_LIST(ID_WPBC, "50789/50791/50785/50787/Z2237/Z1214");
AC_LIST(ID_WPCD, "50797/50799");
AC_LIST(ID_YB400, "50823/50825/Z2227/Z1199/50891/50893/50907/50909/50947/50945/50949/50951");
AC_LIST(ID_PE400B3, "11723/11727");
AC_LIST(ID_J8, "Z2903/Z1903/50113/50111");
AC_LIST(ID_YB301, "50861/50863/Z1215/Z2238/Z1223/Z2246/50875/50873/50877/50879/Z1224/Z2247/50917/50919/Z1241/50921/50923/Z1242/50933/50935/50941/50943/Z1264");
AC_LIST(ID_WYS, "50895/50897/Z1244");
AC_LIST(ID_1TO1, "96901/96902/96903/96904/96135");
AC_LIST(ID_YB201, "50869/50871/Z1226/Z2249/Z1227/Z2250/50889/50887/50925/50927/Z1243");
AC_LIST(ID_PF200, "11963/11961/Z2245/Z1222/11981/11983/Z1234/Z2258");
AC_LIST(ID_GM100, "11967/11965/Z1229/Z2252");
AC_LIST(ID_WOW, "11979/11977/Z1235/Z2259/Z2266");
AC_LIST(ID_1TON, "PD004");
AC_LIST(ID_S10, "20003/20001/Z1904/Z2904");
AC_LIST(ID_FA100, "12037/12035/Z1261/Z1262");
AC_LIST(ID_FA200, "12039/12041/Z1263");
AC_LIST(ID_W10, "60001/60003");
AC_LIST(ID_WXDF, "12070/12072");
AC_LIST(ID_YB100, "50939/Z1259");
AC_LIST(ID_MQ200, "Z2270/Z2269/Z1249/Z1248/12065/12067");
AC_LIST(ID_GW10, "Z1908/20017");

AC_NAME(QA100);
AC_NAME(SA100);
AC_NAME(SA200);
AC_NAME(SA300);
AC_NAME(YA);
AC_NAME(26YA);
AC_NAME(YAB3);
AC_NAME(WJABC);
AC_NAME(YA100);
AC_NAME(CJ200);
AC_NAME(WYA);
AC_NAME(WPA);
AC_NAME(LBB2);
AC_NAME(LBB3);
AC_NAME(PA400);
AC_NAME(WEA);
AC_NAME(CA);
AC_NAME(TAB3);
AC_NAME(TAB12);
AC_NAME(KHB1);
AC_NAME(CACP);
AC_NAME(ZA300);
AC_NAME(ZA300B3);
AC_NAME(ZB300);
AC_NAME(ZB300YJ);
AC_NAME(YA300);
AC_NAME(YA201);
AC_NAME(YA200);
AC_NAME(YA301);
AC_NAME(YA302);
AC_NAME(PC400B23);
AC_NAME(WXA);
AC_NAME(WJ7);
AC_NAME(WYB);
AC_NAME(PCB50);
AC_NAME(YB200);
AC_NAME(DA100);
AC_NAME(YB200TEST);
AC_NAME(WXD);
AC_NAME(YB300);
AC_NAME(WDAA3);
AC_NAME(WDAD3);
AC_NAME(WYAD2D3);
AC_NAME(WPAD3);
AC_NAME(DA400B3);
AC_NAME(PC400B1);
AC_NAME(DA200300);
AC_NAME(DA400D3);
AC_NAME(DA400BP);
AC_NAME(DA400DP);
AC_NAME(PA400B3);
AC_NAME(J9);
AC_NAME(YA400D2D3);
AC_NAME(QA301B1);
AC_NAME(WCBA3);
AC_NAME(IQ100);
AC_NAME(DA100Z);
AC_NAME(IQ300);
AC_NAME(J7);
AC_NAME(WPBC);
AC_NAME(WPCD);
AC_NAME(YB400);
AC_NAME(PE400B3);
AC_NAME(J8);
AC_NAME(YA400B2);
AC_NAME(YB301);
AC_NAME(WYS);
AC_NAME(1TO1);
AC_NAME(1TON);
AC_NAME(YB201);
AC_NAME(PF200);
AC_NAME(GM100);
AC_NAME(WOW);
AC_NAME(S10);
AC_NAME(FA100);
AC_NAME(FA200);
AC_NAME(W10);
AC_NAME(WXDF);
AC_NAME(YB100);
AC_NAME(MQ200);
AC_NAME(GW10);

AC_FUNC(ID_QA100) {
  di.deviceType = DeviceID::ID_QA100;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_SA100) {
  di.deviceType = DeviceID::ID_SA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasHumidityDisplay = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_SA200) {
  di.deviceType = DeviceID::ID_SA200;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_SA300) {
  di.deviceType = DeviceID::ID_SA300;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_YA) {
  di.deviceType = DeviceID::ID_YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_WOW) {
  di.deviceType = DeviceID::ID_WOW;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasFilterScreen = true;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_26YA) {
  di.deviceType = DeviceID::ID_26YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_GM100) {
  di.deviceType = DeviceID::ID_GM100;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_S10) {
  di.deviceType = DeviceID::ID_S10;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_YAB3) {
  di.deviceType = DeviceID::ID_YAB3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_WJABC) {
  di.deviceType = DeviceID::ID_WJABC;
  di.hasUpDownSwipeWind = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_YA100) {
  di.deviceType = DeviceID::ID_YA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
}

AC_FUNC(ID_CJ200) {
  di.deviceType = DeviceID::ID_CJ200;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_WYA) {
  di.deviceType = DeviceID::ID_WYA;
  di.hasSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_WPA) {
  di.deviceType = DeviceID::ID_WPA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

AC_FUNC(ID_LBB2) {
  di.deviceType = DeviceID::ID_LBB2;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_LBB3) {
  di.deviceType = DeviceID::ID_LBB3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

AC_FUNC(ID_PA400) {
  di.deviceType = DeviceID::ID_PA400;
  di.hasUpDownSwipeWind = true;
}

AC_FUNC(ID_WEA) {
  di.deviceType = DeviceID::ID_WEA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_MQ200) {
  di.deviceType = DeviceID::ID_MQ200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasDisney = true;
}

AC_FUNC(ID_CA) {
  di.deviceType = DeviceID::ID_CA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasMyXiaomiBracelet = true;
  di.hasBluetoothUpgrade = true;
}

AC_FUNC(ID_TAB3) {
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

AC_FUNC(ID_TAB12) {
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

AC_FUNC(ID_FA100) {
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

AC_FUNC(ID_FA200) {
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

AC_FUNC(ID_KHB1) {
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

AC_FUNC(ID_J8) {
  di.deviceType = DeviceID::ID_J8;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_CACP) {
  di.deviceType = DeviceID::ID_CACP;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_ZA300) {
  di.deviceType = DeviceID::ID_ZA300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_ZA300B3) {
  di.deviceType = DeviceID::ID_ZA300B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

AC_FUNC(ID_ZB300) {
  di.deviceType = DeviceID::ID_ZB300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

AC_FUNC(ID_ZB300YJ) {
  di.deviceType = DeviceID::ID_ZB300YJ;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

AC_FUNC(ID_YA300) {
  di.deviceType = DeviceID::ID_YA300;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_YA301) {
  di.deviceType = DeviceID::ID_YA301;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_W10) {
  di.deviceType = DeviceID::ID_W10;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
}

AC_FUNC(ID_YA201) {
  di.deviceType = DeviceID::ID_YA201;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_YA200) {
  di.deviceType = DeviceID::ID_YA200;
  di.hasLeftRightSwipeWind = true;
}

AC_FUNC(ID_YA302) {
  di.deviceType = DeviceID::ID_YA302;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

AC_FUNC(ID_PC400B23) {
  di.deviceType = DeviceID::ID_PC400B23;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_WXA) {
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

AC_FUNC(ID_WJ7) {
  di.deviceType = DeviceID::ID_WJ7;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_WYB) {
  di.deviceType = DeviceID::ID_WYB;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

AC_FUNC(ID_PCB50) {
  di.deviceType = DeviceID::ID_PCB50;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_YB200) {
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

AC_FUNC(ID_YB100) {
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

AC_FUNC(ID_DA100) {
  di.deviceType = DeviceID::ID_DA100;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_DA100Z) {
  di.deviceType = DeviceID::ID_DA100Z;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_YB200TEST) {
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

AC_FUNC(ID_WXD) {
  di.deviceType = DeviceID::ID_WXD;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_WXDF) {
  di.deviceType = DeviceID::ID_WXDF;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_YB300) {
  di.deviceType = DeviceID::ID_YB300;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasLadderControl = true;
}

AC_FUNC(ID_WDAA3) {
  di.deviceType = DeviceID::ID_WDAA3;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasECO = true;
}

AC_FUNC(ID_WDAD3) {
  di.deviceType = DeviceID::ID_WDAD3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_WYAD2D3) {
  di.deviceType = DeviceID::ID_WYAD2D3;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

AC_FUNC(ID_WPAD3) {
  di.deviceType = DeviceID::ID_WPAD3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
  di.hasNoPolar = false;
}

AC_FUNC(ID_DA400B3) {
  di.deviceType = DeviceID::ID_DA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_PC400B1) {
  di.deviceType = DeviceID::ID_PC400B1;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_GW10) {
  di.deviceType = DeviceID::ID_GW10;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_PE400B3) {
  di.deviceType = DeviceID::ID_PE400B3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_DA200300) {
  di.deviceType = DeviceID::ID_DA200300;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasECO = true;
}

AC_FUNC(ID_DA400D3) {
  di.deviceType = DeviceID::ID_DA400D3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

AC_FUNC(ID_DA400BP) {
  di.deviceType = DeviceID::ID_DA400BP;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDot5Support = true;
  di.hasPreventStraightLineWind = true;
}

AC_FUNC(ID_DA400DP) {
  di.deviceType = DeviceID::ID_DA400DP;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasOuterDoorDisplay = false;
  di.hasPreventStraightLineWind = true;
}

AC_FUNC(ID_PA400B3) {
  di.deviceType = DeviceID::ID_PA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_J9) {
  di.deviceType = DeviceID::ID_J9;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

AC_FUNC(ID_YA400D2D3) {
  di.deviceType = DeviceID::ID_YA400D2D3;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

AC_FUNC(ID_QA301B1) {
  di.deviceType = DeviceID::ID_QA301B1;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_WCBA3) {
  di.deviceType = DeviceID::ID_WCBA3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_IQ100) {
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

AC_FUNC(ID_IQ300) {
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

AC_FUNC(ID_J7) {
  di.deviceType = DeviceID::ID_J7;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

AC_FUNC(ID_YA400B2) {
  di.deviceType = DeviceID::ID_YA400B2;
  di.hasLeftRightSwipeWind = true;
}

AC_FUNC(ID_WPBC) {
  di.deviceType = DeviceID::ID_WPBC;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

AC_FUNC(ID_WPCD) {
  di.deviceType = DeviceID::ID_WPCD;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

AC_FUNC(ID_YB400) {
  di.deviceType = DeviceID::ID_YB400;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_YB301) {
  di.deviceType = DeviceID::ID_YB301;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
}

AC_FUNC(ID_WYS) {
  di.deviceType = DeviceID::ID_WYS;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasUpDownNoWindFeel = true;
}

AC_FUNC(ID_YB201) {
  di.deviceType = DeviceID::ID_YB201;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
  di.hasLadderControl = true;
}

AC_FUNC(ID_1TO1) {
  di.deviceType = DeviceID::ID_1TO1;
  di.hasElectricHeat = true;
  di.hasDry = true;
  di.hasShow = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasTime1TO1 = true;
}

AC_FUNC(ID_PF200) {
  di.deviceType = DeviceID::ID_PF200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
}

AC_FUNC(ID_1TON) {
  di.deviceType = DeviceID::ID_1TON;
  di.isCentralAC = true;
}

static const DbRecord AC_DBASE[] PROGMEM = {
  AC_RECORD(ID_QA100),     // 0
  AC_RECORD(ID_SA100),     // 1
  AC_RECORD(ID_SA200),     // 2
  AC_RECORD(ID_SA300),     // 3
  AC_RECORD(ID_YA),        // 4
  AC_RECORD(ID_26YA),      // 5
  AC_RECORD(ID_YAB3),      // 6
  AC_RECORD(ID_WJABC),     // 7
  AC_RECORD(ID_YA100),     // 8
  AC_RECORD(ID_CJ200),     // 9
  AC_RECORD(ID_WYA),       // 10
  AC_RECORD(ID_WPA),       // 11
  AC_RECORD(ID_LBB2),      // 12
  AC_RECORD(ID_LBB3),      // 13
  AC_RECORD(ID_PA400),     // 14
  AC_RECORD(ID_WEA),       // 15
  AC_RECORD(ID_CA),        // 16
  AC_RECORD(ID_TAB3),      // 17
  AC_RECORD(ID_TAB12),     // 18
  AC_RECORD(ID_KHB1),      // 19
  AC_RECORD(ID_CACP),      // 20
  AC_RECORD(ID_ZA300),     // 21
  AC_RECORD(ID_ZA300B3),   // 22
  AC_RECORD(ID_ZB300),     // 23
  AC_RECORD(ID_ZB300YJ),   // 24
  AC_RECORD(ID_YA300),     // 25
  AC_RECORD(ID_YA201),     // 26
  AC_RECORD(ID_YA200),     // 27
  AC_RECORD(ID_YA301),     // 28
  AC_RECORD(ID_YA302),     // 29
  AC_RECORD(ID_PC400B23),  // 30
  AC_RECORD(ID_WXA),       // 31
  AC_RECORD(ID_WJ7),       // 32
  AC_RECORD(ID_WYB),       // 33
  AC_RECORD(ID_PCB50),     // 34
  AC_RECORD(ID_YB200),     // 35
  AC_RECORD(ID_DA100),     // 36
  AC_RECORD(ID_YB200TEST), // 37
  AC_RECORD(ID_WXD),       // 38
  AC_RECORD(ID_YB300),     // 39
  AC_RECORD(ID_WDAA3),     // 40
  AC_RECORD(ID_WDAD3),     // 41
  AC_RECORD(ID_WYAD2D3),   // 42
  AC_RECORD(ID_WPAD3),     // 43
  AC_RECORD(ID_DA400B3),   // 44
  AC_RECORD(ID_PC400B1),   // 45
  AC_RECORD(ID_DA200300),  // 46
  AC_RECORD(ID_DA400D3),   // 47
  AC_RECORD(ID_DA400BP),   // 48
  AC_RECORD(ID_DA400DP),   // 49
  AC_RECORD(ID_PA400B3),   // 50
  AC_RECORD(ID_J9),        // 51
  AC_RECORD(ID_YA400D2D3), // 52
  AC_RECORD(ID_QA301B1),   // 53
  AC_RECORD(ID_WCBA3),     // 54
  AC_RECORD(ID_IQ100),     // 55
  AC_RECORD(ID_DA100Z),    // 56
  AC_RECORD(ID_IQ300),     // 57
  AC_RECORD(ID_J7),        // 58
  AC_RECORD(ID_WPBC),      // 59
  AC_RECORD(ID_WPCD),      // 60
  AC_RECORD(ID_YB400),     // 61
  AC_RECORD(ID_PE400B3),   // 62
  AC_RECORD(ID_J8),        // 63
  AC_RECORD(ID_YA400B2),   // 64
  AC_RECORD(ID_YB301),     // 65
  AC_RECORD(ID_WYS),       // 66
  AC_RECORD(ID_1TO1),      // 67
  AC_RECORD(ID_1TON),      // 68
  AC_RECORD(ID_YB201),     // 69
  AC_RECORD(ID_PF200),     // 70
  AC_RECORD(ID_GM100),     // 71
  AC_RECORD(ID_WOW),       // 72
  AC_RECORD(ID_S10),       // 73
  AC_RECORD(ID_FA100),     // 74
  AC_RECORD(ID_FA200),     // 75
  AC_RECORD(ID_W10),       // 76
  AC_RECORD(ID_WXDF),      // 77
  AC_RECORD(ID_YB100),     // 78
  AC_RECORD(ID_MQ200),     // 79
  AC_RECORD(ID_GW10),      // 80
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
  const DbRecord *dbCursor = AC_DBASE;
  for (uint8_t id = 0; id < sizeof(AC_DBASE) / sizeof(AC_DBASE[0]); ++id, ++dbCursor) {
    DbRecord dev;
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
  strncpy_P(ret.id, PSTR("OTHER"), sizeof(ret.id));
  ret.hasUpDownSwipeWind = true;
  ret.hasLeftRightSwipeWind = true;
  return ret;
}
#endif

}  // namespace midea_ac
}  // namespace esphome

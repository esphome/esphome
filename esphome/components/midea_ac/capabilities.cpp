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

static const char LIST_QA100[] PROGMEM = "11005/10977/11007/10979/10563/10567/10711/11415/11417/11423/11425/11467/11469/11471/11473/11475/11477";
static const char LIST_SA100[] PROGMEM = "11175/11177/11419/11421/11437/11439/11441/Z1140";
static const char LIST_SA200[] PROGMEM = "11285/11287/11351/11353/11355/11357";
static const char LIST_SA300[] PROGMEM = "11179/11181/11183/11313/11315/11317/11537/11539/11541/Z1139/Z1149/Z2155/Z1166/Z2185";
static const char LIST_YA[] PROGMEM = "11381/11387/11383/11385/11389/11393/11391/11395/11463/11465/11571/11573/11575/Z2165/Z2168/Z1151/Z1152/Z1153/Z1154/Z1158/Z2165/Z2166/Z2167/Z1167/Z2169/Z2170/Z2172/Z2186/11839/11919/11917/11915/11913";
static const char LIST_26YA[] PROGMEM = "11835";
static const char LIST_YAB3[] PROGMEM = "11397/11399/11401/11403/11405/11407/Z2170/Z1155/Z1156";
static const char LIST_WJABC[] PROGMEM = "11295/11301/11297/11303/11299/11305";
static const char LIST_YA100[] PROGMEM = "50491/50493/50271/50273/50603/50607/50663/50665";
static const char LIST_CJ200[] PROGMEM = "50601/50509/50507/50605/Z1138";
static const char LIST_WYA[] PROGMEM = "50585/50583";
static const char LIST_WPA[] PROGMEM = "50559/50557";
static const char LIST_LBB2[] PROGMEM = "50095/50163/50077/50167";
static const char LIST_LBB3[] PROGMEM = "50119/50213/50397/50081/50285/50403/50661/50695";
static const char LIST_PA400[] PROGMEM = "50387/50401";
static const char LIST_WEA[] PROGMEM = "11373/11375/11377/11379/11309/11311/10983/10985/F0283/F0285/F0287/F0289/F0291/F0293/F0295/F0297/F0299/F0301/F0303/F0305/Z1172/Z1211/Z1212/Z1173";
static const char LIST_CA[] PROGMEM = "11447/11451/11453/11455/11457/11459/11525/11527";
static const char LIST_TAB3[] PROGMEM = "11489/11491/11493/11505/11507/11509/Z1161/Z1162/Z2182/Z2183/11551/11553/11557/11561/11565";
static const char LIST_TAB12[] PROGMEM = "11495/11497/11499/11501/11503/11511/11513/11515/Z1165/Z1164/Z1163/Z2180/Z2181/Z2184/11543/11545/11547/11549/11555/11559/11563/11585/11587/11833/11837/11931/11929/Z2240/Z1217/Z1219/Z2242/11945/11947";
static const char LIST_KHB1[] PROGMEM = "50637/50639/50655/50653/50723";
static const char LIST_CACP[] PROGMEM = "11533/11535";
static const char LIST_ZA300[] PROGMEM = "50199/50201/50265/50269/50307/50373/50317/50375/50431/50433";
static const char LIST_ZA300B3[] PROGMEM = "50251/50253/50281/50289/50309/50315/50383/50385/50427/50429/50669/50701";
static const char LIST_ZB300[] PROGMEM = "50657/50659";
static const char LIST_ZB300YJ[] PROGMEM = "Z1170/Z2191";
static const char LIST_YA300[] PROGMEM = "50205/50207/50393/50451";
static const char LIST_YA301[] PROGMEM = "50531/50533/50677/50687/G0041/G0043/G0047/G0049/G0045/G0051/50777/50779/Z2231/Z1207";
static const char LIST_YA302[] PROGMEM = "50577/50579/50679/50693";
static const char LIST_YA201[] PROGMEM = "50609/50681/50689/50611/50675/50685/50775/50773";
static const char LIST_YA200[] PROGMEM = "50771/50769";
static const char LIST_PC400B23[] PROGMEM = "11367/11369/11371/11323/11327/11445/11449/11461/11757/11759/11761/11323/11325/11327/11445/11449/11461/F0473/Z2217";
static const char LIST_WXA[] PROGMEM = "11695/11697/Z1209/Z2234/12023/12025";
static const char LIST_WJ7[] PROGMEM = "10167/10169/10171";
static const char LIST_WYB[] PROGMEM = "50725/50727";
static const char LIST_PCB50[] PROGMEM = "11719";
static const char LIST_YB200[] PROGMEM = "50731/50733/Z1184/Z2208";
static const char LIST_YB200TEST[] PROGMEM = "55555";
static const char LIST_DA100[] PROGMEM = "11717/11711";
static const char LIST_WXD[] PROGMEM = "11713/11715/Z1208/Z2233";
static const char LIST_YB300[] PROGMEM = "50753/50755/Z1187/Z2212/50841/50839/50835/50837/Z1204/Z2229/50903/Z2254/Z1231/50905/Z1231/Z2254";
static const char LIST_WDAA3[] PROGMEM = "11763/11765/Z2214/11879/11885/Z1210/Z2235/11991/11989/11993/11995/12019/12021";
static const char LIST_WDAD3[] PROGMEM = "11735/11737/11883/11881/11799/11803/11801/11805";
static const char LIST_WYAD2D3[] PROGMEM = "50763/50599/50761/50589/50597/Z1216/Z2239";
static const char LIST_WPAD3[] PROGMEM = "50749/50751/50737/50739/50741/50743";
static const char LIST_DA400B3[] PROGMEM = "11605/11613/11641/11729/11731/11733/Z3063/Z3065/Z2187/Z2215/11843/11231";
static const char LIST_PC400B1[] PROGMEM = "11775/11777/11829/11831/11937/11933";
static const char LIST_DA200300[] PROGMEM = "11589/11591/11593/11597/11599/11649/Z1168/Z2188/Z2164/11739/11741/11743/11745/11747/11749/11661/11663/11667/Z1169/Z2189/11751/11753/11755/11821/11823/11825/11827/Z1191/Z2218/Z1192/Z2219/11891/11889/11925/11927/Z1206/11941/11943/12005/12007/12009/12013/12011/12013/12011";
static const char LIST_DA400D3[] PROGMEM = "11779/11781/11783/11785/11787/11789/11791/11793";
static const char LIST_DA400BP[] PROGMEM = "50853/50847/Z2241/Z1218";
static const char LIST_DA400DP[] PROGMEM = "50843/50845/50849/50851";
static const char LIST_PA400B3[] PROGMEM = "50745/50747";
static const char LIST_J9[] PROGMEM = "19003/19001/Z2900/Z1900/Z1902/19013/19014/19011/19012/Z2902/19017/19015";
static const char LIST_YA400D2D3[] PROGMEM = "50569/50571/50587/50593/50757/50759/50647/50648/50649";
static const char LIST_QA301B1[] PROGMEM = "11795/11797/11796/11798/Z2213/Z1188";
static const char LIST_WCBA3[] PROGMEM = "F0275/F0277/F0279/F0281/F0307/F0309/F0311/F0313/F0315/F0317/11701/11699/11987/11985";
static const char LIST_IQ100[] PROGMEM = "Z1194/11819/11813/Z2221/Z1198";
static const char LIST_DA100Z[] PROGMEM = "11809/11811/Z1193/Z2220/11907/11905/11911/11909/11895/11893/11923/11921/Z1201/Z2232";
static const char LIST_IQ300[] PROGMEM = "Z1195/Z2222/11815/11817";
static const char LIST_J7[] PROGMEM = "Z2901/Z1008/Z1901/50103/50101/50079/50081/50781/50783/50109/50107/Z1205/Z2230";
static const char LIST_YA400B2[] PROGMEM = "50565/50567/50613/50621/50683/50691";
static const char LIST_WPBC[] PROGMEM = "50789/50791/50785/50787/Z2237/Z1214";
static const char LIST_WPCD[] PROGMEM = "50797/50799";
static const char LIST_YB400[] PROGMEM = "50823/50825/Z2227/Z1199/50891/50893/50907/50909/50947/50945/50949/50951";
static const char LIST_PE400B3[] PROGMEM = "11723/11727";
static const char LIST_J8[] PROGMEM = "Z2903/Z1903/50113/50111";
static const char LIST_YB301[] PROGMEM = "50861/50863/Z1215/Z2238/Z1223/Z2246/50875/50873/50877/50879/Z1224/Z2247/50917/50919/Z1241/50921/50923/Z1242/50933/50935/50941/50943/Z1264";
static const char LIST_WYS[] PROGMEM = "50895/50897/Z1244";
static const char LIST_1TO1[] PROGMEM = "96901/96902/96903/96904/96135";
static const char LIST_YB201[] PROGMEM = "50869/50871/Z1226/Z2249/Z1227/Z2250/50889/50887/50925/50927/Z1243";
static const char LIST_PF200[] PROGMEM = "11963/11961/Z2245/Z1222/11981/11983/Z1234/Z2258";
static const char LIST_GM100[] PROGMEM = "11967/11965/Z1229/Z2252";
static const char LIST_WOW[] PROGMEM = "11979/11977/Z1235/Z2259/Z2266";
static const char LIST_1TON[] PROGMEM = "PD004";
static const char LIST_S10[] PROGMEM = "20003/20001/Z1904/Z2904";
static const char LIST_FA100[] PROGMEM = "12037/12035/Z1261/Z1262";
static const char LIST_FA200[] PROGMEM = "12039/12041/Z1263";
static const char LIST_W10[] PROGMEM = "60001/60003";
static const char LIST_WXDF[] PROGMEM = "12070/12072";
static const char LIST_YB100[] PROGMEM = "50939/Z1259";
static const char LIST_MQ200[] PROGMEM = "Z2270/Z2269/Z1249/Z1248/12065/12067";
static const char LIST_GW10[] PROGMEM = "Z1908/20017";

static const char DT_QA100[] PROGMEM = "QA100";
static const char DT_SA100[] PROGMEM = "SA100";
static const char DT_SA200[] PROGMEM = "SA200";
static const char DT_SA300[] PROGMEM = "SA300";
static const char DT_YA[] PROGMEM = "YA";
static const char DT_26YA[] PROGMEM = "26YA";
static const char DT_YAB3[] PROGMEM = "YAB3";
static const char DT_WJABC[] PROGMEM = "WJABC";
static const char DT_YA100[] PROGMEM = "YA100";
static const char DT_CJ200[] PROGMEM = "CJ200";
static const char DT_WYA[] PROGMEM = "WYA";
static const char DT_WPA[] PROGMEM = "WPA";
static const char DT_LBB2[] PROGMEM = "LBB2";
static const char DT_LBB3[] PROGMEM = "LBB3";
static const char DT_PA400[] PROGMEM = "PA400";
static const char DT_WEA[] PROGMEM = "WEA";
static const char DT_CA[] PROGMEM = "CA";
static const char DT_TAB3[] PROGMEM = "TAB3";
static const char DT_TAB12[] PROGMEM = "TAB12";
static const char DT_KHB1[] PROGMEM = "KHB1";
static const char DT_CACP[] PROGMEM = "CACP";
static const char DT_ZA300[] PROGMEM = "ZA300";
static const char DT_ZA300B3[] PROGMEM = "ZA300B3";
static const char DT_ZB300[] PROGMEM = "ZB300";
static const char DT_ZB300YJ[] PROGMEM = "ZB300YJ";
static const char DT_YA300[] PROGMEM = "YA300";
static const char DT_YA201[] PROGMEM = "YA201";
static const char DT_YA200[] PROGMEM = "YA200";
static const char DT_YA301[] PROGMEM = "YA301";
static const char DT_YA302[] PROGMEM = "YA302";
static const char DT_PC400B23[] PROGMEM = "PC400B23";
static const char DT_WXA[] PROGMEM = "WXA";
static const char DT_WJ7[] PROGMEM = "WJ7";
static const char DT_WYB[] PROGMEM = "WYB";
static const char DT_PCB50[] PROGMEM = "PCB50";
static const char DT_YB200[] PROGMEM = "YB200";
static const char DT_DA100[] PROGMEM = "DA100";
static const char DT_YB200TEST[] PROGMEM = "YB200Test";
static const char DT_WXD[] PROGMEM = "WXD";
static const char DT_YB300[] PROGMEM = "YB300";
static const char DT_WDAA3[] PROGMEM = "WDAA3";
static const char DT_WDAD3[] PROGMEM = "WDAD3";
static const char DT_WYAD2D3[] PROGMEM = "WYAD2D3";
static const char DT_WPAD3[] PROGMEM = "WPAD3";
static const char DT_DA400B3[] PROGMEM = "DA400B3";
static const char DT_PC400B1[] PROGMEM = "PC400B1";
static const char DT_DA200300[] PROGMEM = "DA200300";
static const char DT_DA400D3[] PROGMEM = "DA400D3";
static const char DT_DA400BP[] PROGMEM = "DA400BP";
static const char DT_DA400DP[] PROGMEM = "DA400DP";
static const char DT_PA400B3[] PROGMEM = "PA400B3";
static const char DT_J9[] PROGMEM = "J9";
static const char DT_YA400D2D3[] PROGMEM = "YA400D2D3";
static const char DT_QA301B1[] PROGMEM = "QA301B1";
static const char DT_WCBA3[] PROGMEM = "WCBA3";
static const char DT_IQ100[] PROGMEM = "IQ100";
static const char DT_DA100Z[] PROGMEM = "DA100Z";
static const char DT_IQ300[] PROGMEM = "IQ300";
static const char DT_J7[] PROGMEM = "J7";
static const char DT_WPBC[] PROGMEM = "WPBC";
static const char DT_WPCD[] PROGMEM = "WPCD";
static const char DT_YB400[] PROGMEM = "YB400";
static const char DT_PE400B3[] PROGMEM = "PE400B3";
static const char DT_J8[] PROGMEM = "J8";
static const char DT_YA400B2[] PROGMEM = "YA400B2";
static const char DT_YB301[] PROGMEM = "YB301";
static const char DT_WYS[] PROGMEM = "WYS";
static const char DT_1TO1[] PROGMEM = "1TO1";
static const char DT_1TON[] PROGMEM = "1TOn";
static const char DT_YB201[] PROGMEM = "YB201";
static const char DT_PF200[] PROGMEM = "PF200";
static const char DT_GM100[] PROGMEM = "GM100";
static const char DT_WOW[] PROGMEM = "WOW";
static const char DT_S10[] PROGMEM = "S10";
static const char DT_FA100[] PROGMEM = "FA100";
static const char DT_FA200[] PROGMEM = "FA200";
static const char DT_W10[] PROGMEM = "W10";
static const char DT_WXDF[] PROGMEM = "WXDF";
static const char DT_YB100[] PROGMEM = "YB100";
static const char DT_MQ200[] PROGMEM = "MQ200";
static const char DT_GW10[] PROGMEM = "GW10";
static const char DT_OTHER[] PROGMEM = "OTHER";



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


#define CAPABILITIES_FUNC(name) static void DI_##name(DeviceInfo &di)

CAPABILITIES_FUNC(QA100) {
  di.deviceType = DeviceType::QA100;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(SA100) {
  di.deviceType = DeviceType::SA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasHumidityDisplay = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(SA200) {
  di.deviceType = DeviceType::SA200;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasPMV = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(SA300) {
  di.deviceType = DeviceType::SA300;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(YA) {
  di.deviceType = DeviceType::YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(WOW) {
  di.deviceType = DeviceType::WOW;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasFilterScreen = true;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(26YA) {
  di.deviceType = DeviceType::YA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(GM100) {
  di.deviceType = DeviceType::GM100;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(S10) {
  di.deviceType = DeviceType::GM100;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(YAB3) {
  di.deviceType = DeviceType::YAB3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(WJABC) {
  di.deviceType = DeviceType::WJABC;
  di.hasUpDownSwipeWind = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(YA100) {
  di.deviceType = DeviceType::YA100;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
  di.hasPowerManager = true;
}

CAPABILITIES_FUNC(CJ200) {
  di.deviceType = DeviceType::CJ200;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(WYA) {
  di.deviceType = DeviceType::WYA;
  di.hasSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(WPA) {
  di.deviceType = DeviceType::WPA;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

CAPABILITIES_FUNC(LBB2) {
  di.deviceType = DeviceType::LBB2;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(LBB3) {
  di.deviceType = DeviceType::LBB3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

CAPABILITIES_FUNC(PA400) {
  di.deviceType = DeviceType::PA400;
  di.hasUpDownSwipeWind = true;
}

CAPABILITIES_FUNC(WEA) {
  di.deviceType = DeviceType::WEA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(MQ200) {
  di.deviceType = DeviceType::MQ200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasChildrenPreventCold = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasDisney = true;
}

CAPABILITIES_FUNC(CA) {
  di.deviceType = DeviceType::CA;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasMyXiaomiBracelet = true;
  di.hasBluetoothUpgrade = true;
}

CAPABILITIES_FUNC(TAB3) {
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

CAPABILITIES_FUNC(TAB12) {
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

CAPABILITIES_FUNC(FA100) {
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

CAPABILITIES_FUNC(FA200) {
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

CAPABILITIES_FUNC(KHB1) {
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

CAPABILITIES_FUNC(J8) {
  di.deviceType = DeviceType::KHB1;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(CACP) {
  di.deviceType = DeviceType::CACP;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(ZA300) {
  di.deviceType = DeviceType::ZA300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(ZA300B3) {
  di.deviceType = DeviceType::ZA300B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

CAPABILITIES_FUNC(ZB300) {
  di.deviceType = DeviceType::ZB300;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

CAPABILITIES_FUNC(ZB300YJ) {
  di.deviceType = DeviceType::ZB300YJ;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSavingPower = true;
  di.hasDot5Support = true;
  di.hasNoPolar = false;
}

CAPABILITIES_FUNC(YA300) {
  di.deviceType = DeviceType::YA300;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(YA301) {
  di.deviceType = DeviceType::YA301;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(W10) {
  di.deviceType = DeviceType::W10;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
}

CAPABILITIES_FUNC(YA201) {
  di.deviceType = DeviceType::YA201;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(YA200) {
  di.deviceType = DeviceType::YA200;
  di.hasLeftRightSwipeWind = true;
}

CAPABILITIES_FUNC(YA302) {
  di.deviceType = DeviceType::YA302;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasPurify = true;
}

CAPABILITIES_FUNC(PC400B23) {
  di.deviceType = DeviceType::PC400B23;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(WXA) {
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

CAPABILITIES_FUNC(WJ7) {
  di.deviceType = DeviceType::WJ7;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(WYB) {
  di.deviceType = DeviceType::WYB;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

CAPABILITIES_FUNC(PCB50) {
  di.deviceType = DeviceType::PCB50;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(YB200) {
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

CAPABILITIES_FUNC(YB100) {
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

CAPABILITIES_FUNC(DA100) {
  di.deviceType = DeviceType::DA100;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(DA100Z) {
  di.deviceType = DeviceType::DA100Z;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(YB200TEST) {
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

CAPABILITIES_FUNC(WXD) {
  di.deviceType = DeviceType::WXD;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(WXDF) {
  //di.deviceType = DeviceType::WXDF;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(YB300) {
  di.deviceType = DeviceType::YB300;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
  di.hasLadderControl = true;
}

CAPABILITIES_FUNC(WDAA3) {
  di.deviceType = DeviceType::WDAA3;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasSleepCurve = true;
  di.hasECO = true;
}

CAPABILITIES_FUNC(WDAD3) {
  di.deviceType = DeviceType::WDAD3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(WYAD2D3) {
  di.deviceType = DeviceType::WYAD2D3;
  di.hasUpSwipeWind = true;
  di.hasDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

CAPABILITIES_FUNC(WPAD3) {
  di.deviceType = DeviceType::WPAD3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
  di.hasNoPolar = false;
}

CAPABILITIES_FUNC(DA400B3) {
  di.deviceType = DeviceType::DA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(PC400B1) {
  di.deviceType = DeviceType::PC400B1;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(GW10) {
  di.deviceType = DeviceType::GW10;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(PE400B3) {
  di.deviceType = DeviceType::PE400B3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasReadyColdOrHot = true;
  di.hasSleepCurve = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(DA200300) {
  di.deviceType = DeviceType::DA200300;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
  di.hasECO = true;
}

CAPABILITIES_FUNC(DA400D3) {
  di.deviceType = DeviceType::DA400D3;
  di.hasUpDownSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasSleepCurve = true;
}

CAPABILITIES_FUNC(DA400BP) {
  di.deviceType = DeviceType::DA400BP;
  di.hasReadyColdOrHot = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDot5Support = true;
  di.hasPreventStraightLineWind = true;
}

CAPABILITIES_FUNC(DA400DP) {
  di.deviceType = DeviceType::DA400DP;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasOuterDoorDisplay = false;
  di.hasPreventStraightLineWind = true;
}

CAPABILITIES_FUNC(PA400B3) {
  di.deviceType = DeviceType::PA400B3;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(J9) {
  //di.deviceType = DeviceType::WJ9;
  di.hasNatureWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
}

CAPABILITIES_FUNC(YA400D2D3) {
  di.deviceType = DeviceType::YA400D2D3;
  di.hasLeftRightSwipeWind = true;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

CAPABILITIES_FUNC(QA301B1) {
  di.deviceType = DeviceType::QA301B1;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(WCBA3) {
  di.deviceType = DeviceType::WCBA3;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(IQ100) {
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

CAPABILITIES_FUNC(IQ300) {
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

CAPABILITIES_FUNC(J7) {
  di.deviceType = DeviceType::J7;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

CAPABILITIES_FUNC(YA400B2) {
  di.deviceType = DeviceType::YA400B2;
  di.hasLeftRightSwipeWind = true;
}

CAPABILITIES_FUNC(WPBC) {
  di.deviceType = DeviceType::WPBC;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
}

CAPABILITIES_FUNC(WPCD) {
  di.deviceType = DeviceType::WPCD;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
  di.hasNoPolar = false;
  di.hasOuterDoorDisplay = false;
  di.hasDeviceExamination = false;
}

CAPABILITIES_FUNC(YB400) {
  di.deviceType = DeviceType::YB400;
  di.hasLeftRightSwipeWind = true;
  di.hasReadyColdOrHot = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(YB301) {
  di.deviceType = DeviceType::YB301;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
}

CAPABILITIES_FUNC(WYS) {
  di.deviceType = DeviceType::WYS;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasUpDownNoWindFeel = true;
}

CAPABILITIES_FUNC(YB201) {
  di.deviceType = DeviceType::YB301;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasUpDownSwipeWind = true;
  di.hasDot5Support = true;
  di.hasChildrenPreventWind = true;
  di.hasUpDownNoWindFeel = true;
  di.hasLadderControl = true;
}

CAPABILITIES_FUNC(1TO1) {
  di.deviceType = DeviceType::ONETOONE;
  di.hasElectricHeat = true;
  di.hasDry = true;
  di.hasShow = false;
  di.hasNoPolar = false;
  di.hasDeviceExamination = false;
  di.hasTime1TO1 = true;
}

CAPABILITIES_FUNC(PF200) {
  di.deviceType = DeviceType::PF200;
  di.hasUpDownSwipeWind = true;
  di.hasECO = true;
  di.hasSleepCurve = true;
  di.hasReadyColdOrHot = true;
  di.hasLeftRightSwipeWind = true;
  di.hasDot5Support = true;
}

CAPABILITIES_FUNC(1TON) {
  di.deviceType = DeviceType::ONETOONE;
  di.isCentralAC = true;
}
/*
default:

  di.deviceType = DeviceType::OTHER;
  di.hasUpDownSwipeWind = true;
  di.hasLeftRightSwipeWind = true;
}

*/

struct table_t {
  const char *list;
  const char *name;
  std::function<void(DeviceInfo &)> func;
};

#define DEV_STRUCT(name) { LIST_##name, DT_##name, DI_##name }

static const table_t DEVICE_LISTS[] PROGMEM = {
  DEV_STRUCT(QA100),     // 0
  DEV_STRUCT(SA100),     // 1
  DEV_STRUCT(SA200),     // 2
  DEV_STRUCT(SA300),     // 3
  DEV_STRUCT(YA),        // 4
  DEV_STRUCT(26YA),      // 5
  DEV_STRUCT(YAB3),      // 6
  DEV_STRUCT(WJABC),     // 7
  DEV_STRUCT(YA100),     // 8
  DEV_STRUCT(CJ200),     // 9
  DEV_STRUCT(WYA),       // 10
  DEV_STRUCT(WPA),       // 11
  DEV_STRUCT(LBB2),      // 12
  DEV_STRUCT(LBB3),      // 13
  DEV_STRUCT(PA400),     // 14
  DEV_STRUCT(WEA),       // 15
  DEV_STRUCT(CA),        // 16
  DEV_STRUCT(TAB3),      // 17
  DEV_STRUCT(TAB12),     // 18
  DEV_STRUCT(KHB1),      // 19
  DEV_STRUCT(CACP),      // 20
  DEV_STRUCT(ZA300),     // 21
  DEV_STRUCT(ZA300B3),   // 22
  DEV_STRUCT(ZB300),     // 23
  DEV_STRUCT(ZB300YJ),   // 24
  DEV_STRUCT(YA300),     // 25
  DEV_STRUCT(YA201),     // 26
  DEV_STRUCT(YA200),     // 27
  DEV_STRUCT(YA301),     // 28
  DEV_STRUCT(YA302),     // 29
  DEV_STRUCT(PC400B23),  // 30
  DEV_STRUCT(WXA),       // 31
  DEV_STRUCT(WJ7),       // 32
  DEV_STRUCT(WYB),       // 33
  DEV_STRUCT(PCB50),     // 34
  DEV_STRUCT(YB200),     // 35
  DEV_STRUCT(DA100),     // 36
  DEV_STRUCT(YB200TEST), // 37
  DEV_STRUCT(WXD),       // 38
  DEV_STRUCT(YB300),     // 39
  DEV_STRUCT(WDAA3),     // 40
  DEV_STRUCT(WDAD3),     // 41
  DEV_STRUCT(WYAD2D3),   // 42
  DEV_STRUCT(WPAD3),     // 43
  DEV_STRUCT(DA400B3),   // 44
  DEV_STRUCT(PC400B1),   // 45
  DEV_STRUCT(DA200300),  // 46
  DEV_STRUCT(DA400D3),   // 47
  DEV_STRUCT(DA400BP),   // 48
  DEV_STRUCT(DA400DP),   // 49
  DEV_STRUCT(PA400B3),   // 50
  DEV_STRUCT(J9),        // 51
  DEV_STRUCT(YA400D2D3), // 52
  DEV_STRUCT(QA301B1),   // 53
  DEV_STRUCT(WCBA3),     // 54
  DEV_STRUCT(IQ100),     // 55
  DEV_STRUCT(DA100Z),    // 56
  DEV_STRUCT(IQ300),     // 57
  DEV_STRUCT(J7),        // 58
  DEV_STRUCT(WPBC),      // 59
  DEV_STRUCT(WPCD),      // 60
  DEV_STRUCT(YB400),     // 61
  DEV_STRUCT(PE400B3),   // 62
  DEV_STRUCT(J8),        // 63
  DEV_STRUCT(YA400B2),   // 64
  DEV_STRUCT(YB301),     // 65
  DEV_STRUCT(WYS),       // 66
  DEV_STRUCT(1TO1),      // 67
  DEV_STRUCT(1TON),      // 68
  DEV_STRUCT(YB201),     // 69
  DEV_STRUCT(PF200),     // 70
  DEV_STRUCT(GM100),     // 71
  DEV_STRUCT(WOW),       // 72
  DEV_STRUCT(S10),       // 73
  DEV_STRUCT(FA100),     // 74
  DEV_STRUCT(FA200),     // 75
  DEV_STRUCT(W10),       // 76
  DEV_STRUCT(WXDF),      // 77
  DEV_STRUCT(YB100),     // 78
  DEV_STRUCT(MQ200),     // 79
  DEV_STRUCT(GW10),      // 80
};

void DeviceInfo::read(const std::string &sn) {
  for (auto s : extraDeviceTypeForSelfClean)
  {
    
  }
  
}






}  // namespace midea_ac
}  // namespace esphome

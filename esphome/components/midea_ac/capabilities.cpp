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

static const char subQA100List[] PROGMEM = "11005/10977/11007/10979/10563/10567/10711/11415/11417/11423/11425/11467/11469/11471/11473/11475/11477";
static const char subSA100List[] PROGMEM = "11175/11177/11419/11421/11437/11439/11441/Z1140";
static const char subSA200List[] PROGMEM = "11285/11287/11351/11353/11355/11357";
static const char subSA300List[] PROGMEM = "11179/11181/11183/11313/11315/11317/11537/11539/11541/Z1139/Z1149/Z2155/Z1166/Z2185";
static const char subYAList[] PROGMEM = "11381/11387/11383/11385/11389/11393/11391/11395/11463/11465/11571/11573/11575/Z2165/Z2168/Z1151/Z1152/Z1153/Z1154/Z1158/Z2165/Z2166/Z2167/Z1167/Z2169/Z2170/Z2172/Z2186/11839/11919/11917/11915/11913";
static const char sub26YAList[] PROGMEM = "11835";
static const char subYAB3List[] PROGMEM = "11397/11399/11401/11403/11405/11407/Z2170/Z1155/Z1156";
static const char subWJABCList[] PROGMEM = "11295/11301/11297/11303/11299/11305";
static const char subYA100List[] PROGMEM = "50491/50493/50271/50273/50603/50607/50663/50665";
static const char subCJ200List[] PROGMEM = "50601/50509/50507/50605/Z1138";
static const char subWYAList[] PROGMEM = "50585/50583";
static const char subWPAList[] PROGMEM = "50559/50557";
static const char subLBB2List[] PROGMEM = "50095/50163/50077/50167";
static const char subLBB3List[] PROGMEM = "50119/50213/50397/50081/50285/50403/50661/50695";
static const char subPA400List[] PROGMEM = "50387/50401";
static const char subWEAList[] PROGMEM = "11373/11375/11377/11379/11309/11311/10983/10985/F0283/F0285/F0287/F0289/F0291/F0293/F0295/F0297/F0299/F0301/F0303/F0305/Z1172/Z1211/Z1212/Z1173";
static const char subCAList[] PROGMEM = "11447/11451/11453/11455/11457/11459/11525/11527";
static const char subTAB3List[] PROGMEM = "11489/11491/11493/11505/11507/11509/Z1161/Z1162/Z2182/Z2183/11551/11553/11557/11561/11565";
static const char subTAB12List[] PROGMEM = "11495/11497/11499/11501/11503/11511/11513/11515/Z1165/Z1164/Z1163/Z2180/Z2181/Z2184/11543/11545/11547/11549/11555/11559/11563/11585/11587/11833/11837/11931/11929/Z2240/Z1217/Z1219/Z2242/11945/11947";
static const char subKHB1List[] PROGMEM = "50637/50639/50655/50653/50723";
static const char subCACPList[] PROGMEM = "11533/11535";
static const char subZA300List[] PROGMEM = "50199/50201/50265/50269/50307/50373/50317/50375/50431/50433";
static const char subZA300B3List[] PROGMEM = "50251/50253/50281/50289/50309/50315/50383/50385/50427/50429/50669/50701";
static const char subZB300List[] PROGMEM = "50657/50659";
static const char subZB300YJList[] PROGMEM = "Z1170/Z2191";
static const char subYA300List[] PROGMEM = "50205/50207/50393/50451";
static const char subYA301List[] PROGMEM = "50531/50533/50677/50687/G0041/G0043/G0047/G0049/G0045/G0051/50777/50779/Z2231/Z1207";
static const char subYA302List[] PROGMEM = "50577/50579/50679/50693";
static const char subYA201List[] PROGMEM = "50609/50681/50689/50611/50675/50685/50775/50773";
static const char subYA200List[] PROGMEM = "50771/50769";
static const char subPC400B23List[] PROGMEM = "11367/11369/11371/11323/11327/11445/11449/11461/11757/11759/11761/11323/11325/11327/11445/11449/11461/F0473/Z2217";
static const char subWXAList[] PROGMEM = "11695/11697/Z1209/Z2234/12023/12025";
static const char subWJ7List[] PROGMEM = "10167/10169/10171";
static const char subWYBList[] PROGMEM = "50725/50727";
static const char subPCB50List[] PROGMEM = "11719";
static const char subYB200List[] PROGMEM = "50731/50733/Z1184/Z2208";
static const char subYB200TestList[] PROGMEM = "55555";
static const char subDA100List[] PROGMEM = "11717/11711";
static const char subWXDList[] PROGMEM = "11713/11715/Z1208/Z2233";
static const char subYB300List[] PROGMEM = "50753/50755/Z1187/Z2212/50841/50839/50835/50837/Z1204/Z2229/50903/Z2254/Z1231/50905/Z1231/Z2254";
static const char subWDAA3List[] PROGMEM = "11763/11765/Z2214/11879/11885/Z1210/Z2235/11991/11989/11993/11995/12019/12021";
static const char subWDAD3List[] PROGMEM = "11735/11737/11883/11881/11799/11803/11801/11805";
static const char subWYAD2D3List[] PROGMEM = "50763/50599/50761/50589/50597/Z1216/Z2239";
static const char subWPAD3List[] PROGMEM = "50749/50751/50737/50739/50741/50743";
static const char subDA400B3List[] PROGMEM = "11605/11613/11641/11729/11731/11733/Z3063/Z3065/Z2187/Z2215/11843/11231";
static const char subPC400B1List[] PROGMEM = "11775/11777/11829/11831/11937/11933";
static const char subDA200300List[] PROGMEM = "11589/11591/11593/11597/11599/11649/Z1168/Z2188/Z2164/11739/11741/11743/11745/11747/11749/11661/11663/11667/Z1169/Z2189/11751/11753/11755/11821/11823/11825/11827/Z1191/Z2218/Z1192/Z2219/11891/11889/11925/11927/Z1206/11941/11943/12005/12007/12009/12013/12011/12013/12011";
static const char subDA400D3List[] PROGMEM = "11779/11781/11783/11785/11787/11789/11791/11793";
static const char subDA400BP[] PROGMEM = "50853/50847/Z2241/Z1218";
static const char subDA400DP[] PROGMEM = "50843/50845/50849/50851";
static const char subPA400B3List[] PROGMEM = "50745/50747";
static const char subJ9List[] PROGMEM = "19003/19001/Z2900/Z1900/Z1902/19013/19014/19011/19012/Z2902/19017/19015";
static const char subYA400D2D3List[] PROGMEM = "50569/50571/50587/50593/50757/50759/50647/50648/50649";
static const char subQA301B1List[] PROGMEM = "11795/11797/11796/11798/Z2213/Z1188";
static const char subWCBA3List[] PROGMEM = "F0275/F0277/F0279/F0281/F0307/F0309/F0311/F0313/F0315/F0317/11701/11699/11987/11985";
static const char subIQ100List[] PROGMEM = "Z1194/11819/11813/Z2221/Z1198";
static const char subDA100ZList[] PROGMEM = "11809/11811/Z1193/Z2220/11907/11905/11911/11909/11895/11893/11923/11921/Z1201/Z2232";
static const char subIQ300List[] PROGMEM = "Z1195/Z2222/11815/11817";
static const char subJ7List[] PROGMEM = "Z2901/Z1008/Z1901/50103/50101/50079/50081/50781/50783/50109/50107/Z1205/Z2230";
static const char subYA400B2List[] PROGMEM = "50565/50567/50613/50621/50683/50691";
static const char subWPBCList[] PROGMEM = "50789/50791/50785/50787/Z2237/Z1214";
static const char subWPCDList[] PROGMEM = "50797/50799";
static const char subYB400List[] PROGMEM = "50823/50825/Z2227/Z1199/50891/50893/50907/50909/50947/50945/50949/50951";
static const char subPE400B3List[] PROGMEM = "11723/11727";
static const char subJ8List[] PROGMEM = "Z2903/Z1903/50113/50111";
static const char subYB301List[] PROGMEM = "50861/50863/Z1215/Z2238/Z1223/Z2246/50875/50873/50877/50879/Z1224/Z2247/50917/50919/Z1241/50921/50923/Z1242/50933/50935/50941/50943/Z1264";
static const char subWYSList[] PROGMEM = "50895/50897/Z1244";
static const char sub1TO1List[] PROGMEM = "96901/96902/96903/96904/96135";
static const char subYB201List[] PROGMEM = "50869/50871/Z1226/Z2249/Z1227/Z2250/50889/50887/50925/50927/Z1243";
static const char subPF200List[] PROGMEM = "11963/11961/Z2245/Z1222/11981/11983/Z1234/Z2258";
static const char subGM100List[] PROGMEM = "11967/11965/Z1229/Z2252";
static const char subWOWList[] PROGMEM = "11979/11977/Z1235/Z2259/Z2266";
static const char sub1TOnList[] PROGMEM = "PD004";
static const char subS10List[] PROGMEM = "20003/20001/Z1904/Z2904";
static const char subFA100List[] PROGMEM = "12037/12035/Z1261/Z1262";
static const char subFA200List[] PROGMEM = "12039/12041/Z1263";
static const char subW10List[] PROGMEM = "60001/60003";
static const char subWXDFList[] PROGMEM = "12070/12072";
static const char subYB100List[] PROGMEM = "50939/Z1259";
static const char subMQ200List[] PROGMEM = "Z2270/Z2269/Z1249/Z1248/12065/12067";
static const char subGW10List[] PROGMEM = "Z1908/20017";

static const char *extraDeviceTypeForSelfClean[] PROGMEM = {
  subJ7List, subDA100ZList, subYB200List, subYAB3List, subYAList,	subLBB3List, subWEAList, subTAB3List,
  subTAB12List, subKHB1List, subZA300B3List, subZB300List, subYA301List, subYA201List, subYA302List, subPC400B23List,
  subWXAList, subWXDList, subYB300List, subWDAA3List, subPC400B1List, subDA200300List, subJ9List, subQA301B1List,
  subJ7List, subWPBCList, subWJ7List, subYB400List, subWYBList, subWCBA3List, subPE400B3List, subDA400B3List,
  subJ8List, subYA400B2List, subDA400BP, subDA400DP, subYB301List, subYA200List, subYB201List, subPF200List,
  subGM100List, sub26YAList, subWYSList, subWOWList, subPCB50List, subS10List, subFA100List, subW10List,
  subWXDFList, subYB100List, subMQ200List, subFA200List, subGW10List,
};

void DeviceInfo::read(const std::string &sn) {
  for (auto s : extraDeviceTypeForSelfClean)
  {
    
  }
  
}



static const char *extraDeviceTypeForKeepWarm[] PROGMEM = {
	subGM100List, subWOWList, subS10List, subFA100List, subWXDFList, subFA200List, subGW10List
};

static const char *extraDeviceTypeForYADot5[] PROGMEM = { subYAList };
static const char *extraDeviceTypeForYADot5YAB3[] PROGMEM = { subYAB3List };
static const char *extraDeviceTypeForYADot5jr26YA[] PROGMEM = { sub26YAList };

static const char *DEVICE_LISTS[] PROGMEM = {
  subQA100List,
  subSA100List,
  subSA200List,
  subSA300List,
  subYAList,
  sub26YAList,
  subYAB3List,
  subWJABCList,
  subYA100List,
  subCJ200List,
  subWYAList,
  subWPAList,
  subLBB2List,
  subLBB3List,
  subPA400List,
  subWEAList,
  subCAList,
  subTAB3List,
  subTAB12List,
  subKHB1List,
  subCACPList,
  subZA300List,
  subZA300B3List,
  subZB300List,
  subZB300YJList,
  subYA300List,
  subYA301List,
  subYA302List,
  subYA201List,
  subYA200List,
  subPC400B23List,
  subWXAList,
  subWJ7List,
  subWYBList,
  subPCB50List,
  subYB200List,
  subYB200TestList,
  subDA100List,
  subWXDList,
  subYB300List,
  subWDAA3List,
  subWDAD3List,
  subWYAD2D3List,
  subWPAD3List,
  subDA400B3List,
  subPC400B1List,
  subDA200300List,
  subDA400D3List,
  subDA400BP,
  subDA400DP,
  subPA400B3List,
  subJ9List,
  subYA400D2D3List,
  subQA301B1List,
  subWCBA3List,
  subIQ100List,
  subDA100ZList,
  subIQ300List,
  subJ7List,
  subYA400B2List,
  subWPBCList,
  subWPCDList,
  subYB400List,
  subPE400B3List,
  subJ8List,
  subYB301List,
  subWYSList,
  sub1TO1List,
  subYB201List,
  subPF200List,
  subGM100List,
  subWOWList,
  sub1TOnList,
  subS10List,
  subFA100List,
  subFA200List,
  subW10List,
  subWXDFList,
  subYB100List,
  subMQ200List,
  subGW10List,
};


}  // namespace midea_ac
}  // namespace esphome

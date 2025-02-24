#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "air_conditioner.h"

namespace esphome {
namespace midea {
namespace ac {

const char *const Constants::TAG = "midea_xye";


static void set_sensor(Sensor *sensor, float value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
    sensor->publish_state(value);
}

template<typename T> void update_property(T &property, const T &value, bool &flag) {
  if (property != value) {
    property = value;
    flag = true;
  }
}


void AirConditioner::control(const ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = call.get_mode().value();  
  if (call.get_target_temperature().has_value())
    this->target_temperature = (int) call.get_target_temperature().value();
  if (call.get_fan_mode().has_value())
    this->fan_mode = call.get_fan_mode().value();
  if (call.get_swing_mode().has_value())
    this->swing_mode = call.get_swing_mode().value();
  if (call.get_preset().has_value())
    this->preset = call.get_preset().value();
  this->publish_state();

  UpdateNextCycle = 1;
}

void AirConditioner::setup() {
  //this->uart_->check_uart_settings(4800, 1, UART_CONFIG_PARITY_NONE, 8);
  this->last_on_mode_ = *this->supported_modes_.begin();
  UpdateNextCycle = 0;
  ForceReadNextCycle = 1;

  //Start up in Auto fan mode (since unit doesn't report it correctly)
  this->fan_mode = ClimateFanMode::CLIMATE_FAN_AUTO;

  //Set interface to Celcius
  setClientCommand(CLIENT_COMMAND_CELCIUS);
  this->uart_->write_array(TXData, TX_LEN);
  this->uart_->flush();
  delay(this->response_timeout);
  uint8_t data;
  while (this->uart_->available())
    this->uart_->read_byte(&data);

}

//TODO: Not sure if we really need this.
void AirConditioner::setPowerState(bool state) {
  if (state) 
    this->mode = this->last_on_mode_;
  else 
    this->mode = ClimateMode::CLIMATE_MODE_OFF;

  UpdateNextCycle = 1;
}

void AirConditioner::setClientCommand(uint8_t command) {
  TXData[0] =  PREAMBLE;
  TXData[1] =  command;
  TXData[2] =  SERVER_ID;
  TXData[3] =  CLIENT_ID;
  TXData[4] =  FROM_CLIENT;
  TXData[5] =  CLIENT_ID;
  TXData[6] =  0;
  TXData[7] =  0;
  TXData[8] =  0;
  TXData[9] =  0;
  TXData[10] =  0;
  TXData[11] =  0;
  TXData[12] =  0;
  TXData[13] =  0xFF-TXData[1];
  TXData[15] =  PROLOGUE;
  TXData[14] = CalculateCRC(TXData, TX_LEN);
}

void AirConditioner::update() {

    if(0==UpdateNextCycle)
    {
      //construct query command
      setClientCommand(CLIENT_COMMAND_QUERY);

    }else
    {
      //construct set command
      setClientCommand(CLIENT_COMMAND_SET);


      //set mode
      switch(this->mode)
      {
        case ClimateMode::CLIMATE_MODE_OFF: TXData[6] =  OP_MODE_OFF; break;
        case ClimateMode::CLIMATE_MODE_HEAT_COOL: TXData[6] =  OP_MODE_AUTO; break;
        case ClimateMode::CLIMATE_MODE_FAN_ONLY: TXData[6] =  OP_MODE_FAN; break;
        case ClimateMode::CLIMATE_MODE_DRY: TXData[6] =  OP_MODE_DRY; break;
        case ClimateMode::CLIMATE_MODE_HEAT: TXData[6] =  OP_MODE_HEAT; break;
        case ClimateMode::CLIMATE_MODE_COOL: TXData[6] =  OP_MODE_COOL; break;
        default: TXData[6] =  OP_MODE_OFF;     
      }
      //set fan mode
      switch(this->fan_mode.value())
      {
        case ClimateFanMode::CLIMATE_FAN_AUTO: TXData[7] =  FAN_MODE_AUTO; break;
        case ClimateFanMode::CLIMATE_FAN_HIGH: TXData[7] =  FAN_MODE_HIGH; break;
        case ClimateFanMode::CLIMATE_FAN_MEDIUM: TXData[7] =  FAN_MODE_MEDIUM; break;
        case ClimateFanMode::CLIMATE_FAN_LOW: TXData[7] =  FAN_MODE_LOW; break;
        default: TXData[7] =  FAN_MODE_AUTO;     
      }
      //set temp 
      TXData[8] =  this->target_temperature;
      //set mode flags
      TXData[12] = \
        ( (this->preset == ClimatePreset::CLIMATE_PRESET_BOOST) * MODE_FLAG_AUX_HEAT) \
        |((this->preset == ClimatePreset::CLIMATE_PRESET_SLEEP) * MODE_FLAG_ECO) \
        |((this->swing_mode != ClimateSwingMode::CLIMATE_SWING_OFF) * MODE_FLAG_SWING) \
        |(0 * MODE_FLAG_VENT);


      //set timer start
      //TODO: This is not tested. If you use it probably want to switch to State.TimerStart so timer doesn't get ovedrridden
      //TXData[9] =  CalculateSetTime(DesiredState.TimerStart);      
      //set timer stop
      //TXData[10] =  CalculateSetTime(DesiredState.TimerStop);
      
      TXData[14] = CalculateCRC(TXData, TX_LEN);
      
      UpdateNextCycle=0;
    }  


    //TODO: Reimplement flow control for manual RS485 flow control chips 
    //digitalWrite(ComControlPin, RS485_TX_PIN_VALUE);
    this->uart_->write_array(TXData, TX_LEN);
    this->uart_->flush();
    delay(this->response_timeout);
    //digitalWrite(ComControlPin, RS485_RX_PIN_VALUE);

    uint8_t i = 0;
    while (this->uart_->available())
    {
      if (i < RX_LEN)
        this->uart_->read_byte(&RXData[i]);
      i++;
    }
    if (i == RX_LEN){
      ParseResponse();
    }
    else {
      ESP_LOGE(Constants::TAG,"Received incorrect message length from AC");
    }
}

uint8_t AirConditioner::CalculateCRC(uint8_t* data, uint8_t len)
{
    uint32_t crc=0;
    for(uint8_t i=0; i<len; i++)
    { 
      if(i!= len - 2)
      {
        crc+=data[i];
      }
    }
    return 0xFF - (crc&0xFF);
}

void AirConditioner::ParseResponse()
{
  // validate the response
  if(
    (PREAMBLE==RXData[RX_BYTE_PREAMBLE])&&\
    (PROLOGUE==RXData[RX_BYTE_PROLOGUE])&&\
    (TO_CLIENT==RXData[RX_BYTE_TO_CLIENT])&&\
    (RXData[RX_BYTE_CRC]==CalculateCRC(RXData, RX_LEN))\
  )
  {
    
    ClimateMode mode = ClimateMode::CLIMATE_MODE_OFF;
    ClimateFanMode fan_mode = ClimateFanMode::CLIMATE_FAN_AUTO;
    ClimatePreset preset = ClimatePreset::CLIMATE_PRESET_NONE;
    
    switch(RXData[RX_BYTE_OP_MODE]) 
    {
      case OP_MODE_OFF: mode = ClimateMode::CLIMATE_MODE_OFF; break;
      case OP_MODE_AUTO: mode = ClimateMode::CLIMATE_MODE_HEAT_COOL; break;
      case OP_MODE_FAN: mode = ClimateMode::CLIMATE_MODE_FAN_ONLY; break;
      case OP_MODE_DRY: mode = ClimateMode::CLIMATE_MODE_DRY; break;
      case OP_MODE_HEAT: mode = ClimateMode::CLIMATE_MODE_HEAT; break;
      case OP_MODE_COOL: mode = ClimateMode::CLIMATE_MODE_COOL; break;
    }        

    switch(RXData[RX_BYTE_FAN_MODE]) 
    {
      case FAN_MODE_HIGH: fan_mode = ClimateFanMode::CLIMATE_FAN_HIGH; break;
      case FAN_MODE_MEDIUM: fan_mode = ClimateFanMode::CLIMATE_FAN_MEDIUM; break;
      case FAN_MODE_LOW: fan_mode = ClimateFanMode::CLIMATE_FAN_LOW; break;
      case FAN_MODE_OFF: fan_mode = ClimateFanMode::CLIMATE_FAN_OFF; break;
    }
    if(RXData[RX_BYTE_FAN_MODE] & FAN_MODE_AUTO)
    {
      fan_mode = ClimateFanMode::CLIMATE_FAN_AUTO;
    }

    if (RXData[RX_BYTE_MODE_FLAGS] & MODE_FLAG_AUX_HEAT)
      preset = ClimatePreset::CLIMATE_PRESET_BOOST;
    else if (RXData[RX_BYTE_MODE_FLAGS] & MODE_FLAG_ECO)
      preset = ClimatePreset::CLIMATE_PRESET_SLEEP;


    bool need_publish = false;
    
    update_property(this->current_temperature, (float)CalculateTemp(RXData[RX_BYTE_T1_TEMP]), need_publish);
    update_property(this->mode, mode, need_publish);
    if( mode != ClimateMode::CLIMATE_MODE_OFF) //Don't update below states unless mode is an ON state
    {
      this->last_on_mode_ = mode;
    }

    if( mode != ClimateMode::CLIMATE_MODE_OFF || ForceReadNextCycle == 1) //Don't update below states unless mode is an ON state
    {
      
      update_property(this->target_temperature, (float)RXData[RX_BYTE_SET_TEMP], need_publish);
      //Don't update fan mode when we set it to auto
      //It seems the heatpump doesn't report back Auto mode - it reports back the current mode
      if(this->fan_mode != fan_mode && this->fan_mode != ClimateFanMode::CLIMATE_FAN_AUTO)
      {
        need_publish = true;
        this->fan_mode = fan_mode;
      }
      if ((this->swing_mode != ClimateSwingMode::CLIMATE_SWING_OFF) != (bool)(RXData[RX_BYTE_MODE_FLAGS] & MODE_FLAG_SWING))
        need_publish = true;
      this->swing_mode = (RXData[RX_BYTE_MODE_FLAGS] & MODE_FLAG_SWING) ? ClimateSwingMode::CLIMATE_SWING_VERTICAL : ClimateSwingMode::CLIMATE_SWING_OFF;
      if (this->preset != preset)
        need_publish = true;
      this->preset = preset;
    }
    

    if (need_publish)
      this->publish_state();

    set_sensor(this->outdoor_sensor_, CalculateTemp(RXData[RX_BYTE_T3_TEMP]) );
    set_sensor(this->temperature_2a_sensor_, CalculateTemp(RXData[RX_BYTE_T2A_TEMP]));
    set_sensor(this->temperature_2b_sensor_, CalculateTemp(RXData[RX_BYTE_T2B_TEMP]));
    set_sensor(this->current_sensor_, RXData[RX_BYTE_CURRENT]);
    set_sensor(this->timer_start_sensor_, CalculateGetTime(RXData[RX_BYTE_TIMER_START]));
    set_sensor(this->timer_stop_sensor_, CalculateGetTime(RXData[RX_BYTE_TIMER_STOP]));
    set_sensor(this->error_flags_sensor_, (RXData[RX_BYTE_ERROR_FLAGS1]<<0) | (RXData[RX_BYTE_ERROR_FLAGS2]<<8));
    set_sensor(this->protect_flags_sensor_, (RXData[RX_BYTE_PROTECT_FLAGS1]<<0) | (RXData[RX_BYTE_PROTECT_FLAGS2]<<8));

  }else
  { 
    ESP_LOGE(Constants::TAG,"Received invalid response from AC");
  }

  ForceReadNextCycle = 0;
}

uint8_t AirConditioner::CalculateSetTime(uint32_t time)
{
  uint32_t current_time = time; 
  uint8_t timeValue=0;
  
  if(0<(current_time/960))
  {
    timeValue |=0x40;
    current_time = current_time % 960;
  }
  if(0<(current_time/480))
  {
    timeValue |=0x20;
    current_time = current_time % 480;
  }
  if(0<(current_time/240))
  {
    timeValue |=0x10;
    current_time = current_time % 240;
  }
  if(0<(current_time/120))
  {
    timeValue |=0x08;
    current_time = current_time % 120;
  }
  if(0<(current_time/60))
  {
    timeValue |=0x04;
    current_time = current_time % 60;
  }
  if(0<(current_time/30))
  {
    timeValue |=0x02;
    current_time = current_time % 30;
  }
  if(0<(current_time/15))
  {
    timeValue |=0x01;
    current_time = current_time % 15;
  }
  return timeValue; 
}

uint32_t AirConditioner::CalculateGetTime(uint8_t time)
{
  uint32_t timeValue=0;
  
  if(time & 0x40)
  {
    timeValue += 960;
  }
  if(time & 0x20)
  {
    timeValue += 480;
  }
  if(time & 0x10)
  {
    timeValue += 240;
  }
  if(time & 0x08)
  {
    timeValue += 120;
  }
  if(time & 0x04)
  {
    timeValue += 60;
  }
  if(time & 0x02)
  {
    timeValue += 30;
  }
  if(time & 0x01)
  {
    timeValue += 15;
  }
  return timeValue; 
}

float AirConditioner::CalculateTemp(uint8_t byte) {
  return (byte-0x30)/2.0;
}

ClimateTraits AirConditioner::traits() {
  auto traits = ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(1.0);
  traits.set_supported_modes(this->supported_modes_);
  traits.set_supported_swing_modes(this->supported_swing_modes_);
  traits.set_supported_presets(this->supported_presets_);
  traits.set_supported_custom_presets(this->supported_custom_presets_);
  traits.set_supported_custom_fan_modes(this->supported_custom_fan_modes_);
  /* + MINIMAL SET OF CAPABILITIES */
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_HIGH);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_OFF); //Can't set it but will be reported

  if (!traits.get_supported_modes().empty())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_OFF);
  if (!traits.get_supported_swing_modes().empty())
    traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_OFF);
  if (!traits.get_supported_presets().empty())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);
  return traits;
}

void AirConditioner::dump_config() {
  
  ESP_LOGCONFIG(Constants::TAG, "MideaXYE:");
  ESP_LOGCONFIG(Constants::TAG, "  [x] Period: %dms", this->get_update_interval());
  ESP_LOGCONFIG(Constants::TAG, "  [x] Response timeout: %dms", this->response_timeout);
#ifdef USE_REMOTE_TRANSMITTER
  ESP_LOGCONFIG(Constants::TAG, "  [x] Using RemoteTransmitter");
#endif
  this->dump_traits_(Constants::TAG);
  
}

/* ACTIONS */

void AirConditioner::do_follow_me(float temperature, bool beeper) {
#ifdef USE_REMOTE_TRANSMITTER
  IrFollowMeData data(static_cast<uint8_t>(lroundf(temperature)), beeper);
  this->transmitter_.transmit(data);
#else
  ESP_LOGW(Constants::TAG, "Action needs remote_transmitter component");
#endif
}

void AirConditioner::do_swing_step() {
#ifdef USE_REMOTE_TRANSMITTER
  IrSpecialData data(0x01);
  this->transmitter_.transmit(data);
#else
  ESP_LOGW(Constants::TAG, "Action needs remote_transmitter component");
#endif
}

void AirConditioner::do_display_toggle() {
#ifdef USE_REMOTE_TRANSMITTER
    IrSpecialData data(0x08);
    this->transmitter_.transmit(data);
#else
    ESP_LOGW(Constants::TAG, "Action needs remote_transmitter component");
#endif
}

}  // namespace ac
}  // namespace midea
}  // namespace esphome

#endif  // USE_ARDUINO

#include "abbaurora.h"

namespace esphome {
namespace abbaurora {

static const char *TAG = "abbaurora";

void ABBAuroraComponent::setup() 
{
    ESP_LOGD(TAG, "Setup");

    // Set flowcontrolpin
    if (this->flow_control_pin_ != nullptr)
        this->flow_control_pin_->setup();

    for( int i=0; i<8; i++ ) receive_data_[i]=0;

    connection_status_->publish_state("Setup");
}

void ABBAuroraComponent::dump_config() 
{
    if (this->flow_control_pin_ != nullptr) {
        LOG_PIN("  Flow control Pin: ", this->flow_control_pin_);
    }
    ESP_LOGCONFIG(TAG, "  Inverter Address: %d", this->address__ );
}

void ABBAuroraComponent::loop()
{
    const uint32_t now = millis();
    static uint32_t last_request = 0;
    static uint32_t rotaterequests = 0;
      
    if( now - last_request > 1000)
    {
        last_request = now;
        rotaterequests++;
        switch( rotaterequests % 30)
        {  
            case 2: if(connection_status_ && this->read_state_()) // If inverter is connected
                        connection_status_->publish_state( ABBAuroraComponent::inverter_state_text_(State_.InverterState) );
                    break;
            case 4: if(identification_ && this->read_system_serialnumber_())
                        identification_->publish_state(this->SystemSerialNumber_.SerialNumber);
                    break;
            case 6: if(version_ && this->read_version_())
                        version_->publish_state( this->Version_.Par1 );
                    break;        
            case 8: if(temperature_inverter_ && this->read_dsp_value_(TEMPERATURE_INVERTER, MODULE_MEASUREMENT))
                        temperature_inverter_->publish_state(this->DSP_.Value);
                    break;
            case 10:if(power_in_1_ && this->read_dsp_value_(POWER_IN_1, MODULE_MEASUREMENT))
                        power_in_1_->publish_state(this->DSP_.Value);
                    if(power_in_total_ && power_in_1_ && power_in_2_)
                        power_in_total_->publish_state(power_in_1_->get_state() + power_in_2_->get_state());
                    break;
            case 12:if(power_in_2_ && this->read_dsp_value_(POWER_IN_2, MODULE_MEASUREMENT))
                        power_in_2_->publish_state(this->DSP_.Value);
                    if(power_in_total_ && power_in_1_ && power_in_2_)
                        power_in_total_->publish_state(power_in_1_->get_state() + power_in_2_->get_state());
                    break;
            case 14:if(v_in_1_ && this->read_dsp_value_(V_IN_1, MODULE_MEASUREMENT))
                        v_in_1_->publish_state(this->DSP_.Value);
                    break;
            case 16:if(v_in_2_ && this->read_dsp_value_(V_IN_2, MODULE_MEASUREMENT))
                        v_in_2_->publish_state(this->DSP_.Value);
                    break;
            case 18:if(i_in_1_ && this->read_dsp_value_(I_IN_1, MODULE_MEASUREMENT))
                        i_in_1_->publish_state(this->DSP_.Value);
                    break;
            case 20:if(i_in_2_ && this->read_dsp_value_(I_IN_2, MODULE_MEASUREMENT))
                        i_in_2_->publish_state(this->DSP_.Value);
                    break;
            case 22:if(temperature_booster_ && this->read_dsp_value_(TEMPERATURE_BOOSTER, MODULE_MEASUREMENT))
                        temperature_booster_->publish_state(this->DSP_.Value);
                    break;
            case 1:
            case 11:
            case 21:if(grid_power_ && this->read_dsp_value_(GRID_POWER, MODULE_MEASUREMENT))
                        grid_power_->publish_state( this->DSP_.Value );
                    break;
            case 3:
            case 13:
            case 23:if(grid_voltage_ && this->read_dsp_value_(GRID_VOLTAGE, MODULE_MEASUREMENT))
                        grid_voltage_->publish_state(this->DSP_.Value);
                    break;
            case 5:
            case 15:
            case 25:if(cumulated_energy_total_ && this->read_cumulated_energy_(TOTAL))
                        cumulated_energy_total_->publish_state(this->CumulatedEnergy_.Energy);
                    break;
            case 7:
            case 17:
            case 27:if(cumulated_energy_today_ && this->read_cumulated_energy_(CURRENT_DAY))
                        cumulated_energy_today_->publish_state(this->CumulatedEnergy_.Energy);
                    break;
        }            
    }
}

bool ABBAuroraComponent::send_(uint8_t address, uint8_t param0, uint8_t param1, uint8_t param2, uint8_t param3, uint8_t param4, uint8_t param5, uint8_t param6)
{
    uint8_t SendData[10];
    SendData[0] = address;
    SendData[1] = param0;
    SendData[2] = param1;
    SendData[3] = param2;
    SendData[4] = param3;
    SendData[5] = param4;
    SendData[6] = param5;
    SendData[7] = param6;

    // Calculate CRC16
    uint8_t BccLo = 0xFF; uint8_t BccHi = 0xFF;
    for (int i = 0; i < 8; i++)
    {
        uint8_t New = SendData[i] ^ BccLo;
        uint8_t Tmp = New << 4;
        New = Tmp ^ New; Tmp = New >> 5; BccLo = BccHi; BccHi = New ^ Tmp;
        Tmp = New << 3; BccLo = BccLo ^ Tmp; Tmp = New >> 4; BccLo = BccLo ^ Tmp;
    }
    // CRC bytes
    SendData[8] = (uint8_t)(~BccLo);
    SendData[9] = (uint8_t)(~BccHi);

    // Clear data
    for( int i=0; i<8; i++ ) receive_data_[i]=0;
    // Empty RX buffer
    while( this->available() )
    {
        uint8_t purge;
        this->read_byte( &purge );
    }

    // Flow control to TX
    if (this->flow_control_pin_ != nullptr)
        this->flow_control_pin_->digital_write(true);     

    // Send data
    this->write_array( (uint8_t *)SendData, 10 );
    // Wait until complete
    this->flush();            

    // Flow control to RX
    if (this->flow_control_pin_ != nullptr)
        this->flow_control_pin_->digital_write(false);

    // Wait for 100ms for data to arrive
    const uint32_t now = millis();
    bool datawaiting = false;
    while( millis() - now < 100 && !datawaiting )
        datawaiting = this->available();

    ESP_LOGV( TAG, "Waited for %d ms for data to arrive", millis() - now );

    // Data waiting?
    if( datawaiting )
    {
        // Receive data
        if (this->read_array( (uint8_t *)receive_data_, 8 ) )
        {
            // Calc CRC16
            BccLo = 0xFF; BccHi = 0xFF;
            for (int i = 0; i < 6; i++)
            {
                uint8_t New = receive_data_[i] ^ BccLo;
                uint8_t Tmp = New << 4;
                New = Tmp ^ New; Tmp = New >> 5; BccLo = BccHi; BccHi = New ^ Tmp; 
                Tmp = New << 3; BccLo = BccLo ^ Tmp; Tmp = New >> 4; BccLo = BccLo ^ Tmp;
            }   
            // Check CRC16 
            if(  receive_data_[7] == (uint8_t)(~BccHi) &&  receive_data_[6] == (uint8_t)(~BccLo) )
                return true; // Checksum OK
            else
                ESP_LOGD(TAG, "CRC error in received data");
        }
        else
            ESP_LOGD(TAG, "Failed receiving data");
    }
    else
        ESP_LOGD(TAG, "No data available");
        
    return false;
}

/**
 * Reads a single value of the digital signal procesor.
 * Not all values are supported by all models. 
 * Read values are in following Units:
 * Voltage V
 * Current A 
 * Power W 
 * Temperature °C 
 * 
 **/
bool ABBAuroraComponent::read_dsp_value_(DspValueType type, DspGlobal global)
{
    if ((((int)type >= 1 && (int)type <= 9) || ((int)type >= 21 && (int)type <= 63)) && ((int)global >= 0 && (int)global <= 1))
    {
        DSP_.ReadState = send_(this->address_, (uint8_t)59, type, global, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

        if (DSP_.ReadState == false)
        {
            receive_data_[0] = 255;
            receive_data_[1] = 255;
        }
    }
    else
    {
        DSP_.ReadState = false;
        receive_data_[0] = 255; receive_data_[1] = 255;
        receive_data_[2] = 0; receive_data_[3] = 0; receive_data_[4] = 0; 
        receive_data_[5] = 0; receive_data_[6] = 0; receive_data_[7] = 0;
    }

    DSP_.TransmissionState = receive_data_[0];
    DSP_.GlobalState = receive_data_[1];

    FloatBytes.asBytes[0] = receive_data_[5];
    FloatBytes.asBytes[1] = receive_data_[4];
    FloatBytes.asBytes[2] = receive_data_[3];
    FloatBytes.asBytes[3] = receive_data_[2];
    
    DSP_.Value = FloatBytes.asFloat;
    if( DSP_.Value > 1E10 ||  DSP_.Value < 0 ) // this does not make sense
      DSP_.Value = 0;

    return DSP_.ReadState;
}

bool ABBAuroraComponent::read_timedate_()
{
    TimeDate_.ReadState = send_(this->address_, (uint8_t)70, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    if (TimeDate_.ReadState == false)
    {
        receive_data_[0] = 255;
        receive_data_[1] = 255;
    }

    TimeDate_.TransmissionState = receive_data_[0];
    TimeDate_.GlobalState = receive_data_[1];
    TimeDate_.Seconds = ((unsigned long)receive_data_[2] << 24) + ((unsigned long)receive_data_[3] << 16) + ((unsigned long)receive_data_[4] << 8) + (unsigned long)receive_data_[5];
    return TimeDate_.ReadState;
}

bool ABBAuroraComponent::read_last_four_alarms_()
{
    LastFourAlarms_.ReadState = send_(this->address_, (uint8_t)86, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    if (LastFourAlarms_.ReadState == false)
    {
        receive_data_[0] = 255;
        receive_data_[1] = 255;
        receive_data_[2] = 255;
        receive_data_[3] = 255;
        receive_data_[4] = 255;
        receive_data_[5] = 255;
    }

    LastFourAlarms_.TransmissionState = receive_data_[0];
    LastFourAlarms_.GlobalState = receive_data_[1];
    LastFourAlarms_.Alarms1 = receive_data_[2];
    LastFourAlarms_.Alarms2 = receive_data_[3];
    LastFourAlarms_.Alarms3 = receive_data_[4];
    LastFourAlarms_.Alarms4 = receive_data_[5];

    return LastFourAlarms_.ReadState;
}

bool ABBAuroraComponent::read_junctionbox_state_(uint8_t nj)
{
    return send_(this->address_, (uint8_t)200, nj, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
    /*
    // ToDo not yet implemented
    JunctionBoxState_.State_.transmissionState = receive_data_[0];
    JunctionBoxState_.State_.globalState = -1;

    JunctionBoxState_.jBoxState_.burnFuseOnJBox 		= receive_data_[1] & 0x01;
    JunctionBoxState_.jBoxState_.jBoxNotCalibrated 	= receive_data_[1] & 0x80;
    JunctionBoxState_.jBoxState_.jBoxOvercurrent 		= receive_data_[1] & 0x10;
    JunctionBoxState_.jBoxState_.jBoxOvertemperature 	= receive_data_[1] & 0x02;
    JunctionBoxState_.jBoxState_.jBoxOvervoltage 		= receive_data_[1] & 0x04;
    JunctionBoxState_.jBoxState_.noCommunication 		= receive_data_[1] & 0x40;
    JunctionBoxState_.jBoxState_.powerOff 			= receive_data_[1] & 0x20;
    JunctionBoxState_.jBoxState_.unbalancedStringCurrent = receive_data_[1] & 0x08;
  
    JunctionBoxState_.fuseBurnt[1] = receive_data_[4] & 0x04;
    JunctionBoxState_.fuseBurnt[2] = receive_data_[4] & 0x01;
    JunctionBoxState_.fuseBurnt[3] = receive_data_[3] & 0x40;
    JunctionBoxState_.fuseBurnt[4] = receive_data_[3] & 0x10;
    JunctionBoxState_.fuseBurnt[5] = receive_data_[3] & 0x04;
    JunctionBoxState_.fuseBurnt[6] = receive_data_[3] & 0x01;
    JunctionBoxState_.fuseBurnt[7] = receive_data_[2] & 0x40;
    JunctionBoxState_.fuseBurnt[8] = receive_data_[2] & 0x10;
    JunctionBoxState_.fuseBurnt[9] = receive_data_[2] & 0x04;
    JunctionBoxState_.fuseBurnt[10] = receive_data_[2] & 0x01;
    JunctionBoxState_.fuseBurnt[11] = receive_data_[4] & 0x08;
    JunctionBoxState_.fuseBurnt[12] = receive_data_[4] & 0x02;
    JunctionBoxState_.fuseBurnt[13] = receive_data_[3] & 0x80;
    JunctionBoxState_.fuseBurnt[14] = receive_data_[3] & 0x20;
    JunctionBoxState_.fuseBurnt[15] = receive_data_[3] & 0x08;
    JunctionBoxState_.fuseBurnt[16] = receive_data_[3] & 0x02;
    JunctionBoxState_.fuseBurnt[17] = receive_data_[2] & 0x80;
    JunctionBoxState_.fuseBurnt[18] = receive_data_[2] & 0x20;
    JunctionBoxState_.fuseBurnt[19] = receive_data_[2] & 0x08;
    JunctionBoxState_.fuseBurnt[20] = receive_data_[2] & 0x02;
    JunctionBoxState_.stringCurrentUnbalanced[0] = receive_data_[5] & 0x80;
    JunctionBoxState_.stringCurrentUnbalanced[1] = receive_data_[5] & 0x40;
    JunctionBoxState_.stringCurrentUnbalanced[2] = receive_data_[5] & 0x20;
    JunctionBoxState_.stringCurrentUnbalanced[3] = receive_data_[5] & 0x10;
    JunctionBoxState_.stringCurrentUnbalanced[4] = receive_data_[5] & 0x08;
    JunctionBoxState_.stringCurrentUnbalanced[5] = receive_data_[5] & 0x04;
    JunctionBoxState_.stringCurrentUnbalanced[6] = receive_data_[5] & 0x02;
    JunctionBoxState_.stringCurrentUnbalanced[7] = receive_data_[5] & 0x01;
    JunctionBoxState_.stringCurrentUnbalanced[8] = receive_data_[4] & 0x80;
    JunctionBoxState_.stringCurrentUnbalanced[9] = receive_data_[4] & 0x40;
    */
}

bool ABBAuroraComponent::read_junctionbox_value_(uint8_t nj, uint8_t par)
{
    return send_(this->address_, (uint8_t)201, nj, par, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
}

// Inverters
bool ABBAuroraComponent::read_system_partnumber_(void)
{
    SystemPartNumber_.ReadState = send_(this->address_, (uint8_t)52, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    SystemPartNumber_.PartNumber.assign(1, (char)receive_data_[0] );
    SystemPartNumber_.PartNumber += (char)receive_data_[1];
    SystemPartNumber_.PartNumber += (char)receive_data_[2]; 
    SystemPartNumber_.PartNumber += (char)receive_data_[3];
    SystemPartNumber_.PartNumber += (char)receive_data_[4]; 
    SystemPartNumber_.PartNumber += (char)receive_data_[5]; 
    
    return SystemPartNumber_.ReadState;
}

bool ABBAuroraComponent::read_system_serialnumber_(void)
{
    SystemSerialNumber_.ReadState = send_(this->address_, (uint8_t)63, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    SystemSerialNumber_.SerialNumber.assign(1,(char)receive_data_[0]);
    SystemSerialNumber_.SerialNumber += (char)receive_data_[1];
    SystemSerialNumber_.SerialNumber += (char)receive_data_[2]; 
    SystemSerialNumber_.SerialNumber += (char)receive_data_[3];
    SystemSerialNumber_.SerialNumber += (char)receive_data_[4]; 
    SystemSerialNumber_.SerialNumber += (char)receive_data_[5]; 

    return SystemSerialNumber_.ReadState;
}

bool ABBAuroraComponent::read_manufacturing_week_year_(void)
{
    ManufacturingWeekYear_.ReadState = send_(this->address_, (uint8_t)65, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    if (ManufacturingWeekYear_.ReadState == false)
    {
        receive_data_[0] = 255;
        receive_data_[1] = 255;
    }

    ManufacturingWeekYear_.TransmissionState = receive_data_[0];
    ManufacturingWeekYear_.GlobalState = receive_data_[1];

    ManufacturingWeekYear_.Week.assign(1,(char)receive_data_[2]);  
    ManufacturingWeekYear_.Week += (char)receive_data_[3];
    ManufacturingWeekYear_.Year.assign(1,(char)receive_data_[4]);  
    ManufacturingWeekYear_.Year += (char)receive_data_[5];  

    return ManufacturingWeekYear_.ReadState;
}

bool ABBAuroraComponent::read_firmware_release_(void)
{
    FirmwareRelease_.ReadState = send_(this->address_, (uint8_t)72, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    if (FirmwareRelease_.ReadState == false)
    {
        receive_data_[0] = 255;
        receive_data_[1] = 255;
    }

    FirmwareRelease_.TransmissionState = receive_data_[0];
    FirmwareRelease_.GlobalState = receive_data_[1];

    FirmwareRelease_.Release.assign(1,(char)receive_data_[2]);
    FirmwareRelease_.Release += '.';
    FirmwareRelease_.Release += (char)receive_data_[3];  
    FirmwareRelease_.Release += '.';
    FirmwareRelease_.Release += (char)receive_data_[4];  
    FirmwareRelease_.Release += '.';
    FirmwareRelease_.Release += (char)receive_data_[5];  

    return FirmwareRelease_.ReadState;
}

bool ABBAuroraComponent::read_cumulated_energy_(CumulatedEnergyType par)
{
    if ((int)par >= 0 && (int)par <= 6)
    {
        CumulatedEnergy_.ReadState = send_(this->address_, (uint8_t)78, par, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

        if (CumulatedEnergy_.ReadState == false)
        {
            receive_data_[0] = 255;
            receive_data_[1] = 255;
        }
    }
    else
    {
        CumulatedEnergy_.ReadState = false;
        receive_data_[0] = 255; receive_data_[1] = 255;
        receive_data_[2] = 0; receive_data_[3] = 0; receive_data_[4] = 0; 
        receive_data_[5] = 0; receive_data_[6] = 0; receive_data_[7] = 0;
    }

    CumulatedEnergy_.TransmissionState = receive_data_[0];
    CumulatedEnergy_.GlobalState = receive_data_[1];
    
    if (CumulatedEnergy_.ReadState == true)
    {
        LongBytes.asBytes[0] = receive_data_[5];
        LongBytes.asBytes[1] = receive_data_[4];
        LongBytes.asBytes[2] = receive_data_[3];
        LongBytes.asBytes[3] = receive_data_[2];

        CumulatedEnergy_.Energy = LongBytes.asUlong;
    }
    return CumulatedEnergy_.ReadState;
}

bool ABBAuroraComponent::write_baudrate_setting_(uint8_t baudcode)
{
    if ((int)baudcode >= 0 && (int)baudcode <= 3)
    {
        return send_(this->address_, (uint8_t)85, baudcode, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
    }
    else
    {
        for( int i=0; i<8; i++ ) receive_data_[i]=0;
        return false;
    }
}

// Central
bool ABBAuroraComponent::read_flags_switch_central_()
{
    return send_(this->address_, (uint8_t)67, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
}

bool ABBAuroraComponent::read_cumulated_energy_central_(uint8_t var, uint8_t ndays_h, uint8_t ndays_l, uint8_t global)
{
    return send_(this->address_, (uint8_t)68, var, ndays_h, ndays_l, global, (uint8_t)0, (uint8_t)0);
}

bool ABBAuroraComponent::read_firmware_release_central_(uint8_t var)
{
    return send_(this->address_, (uint8_t)72, var, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
}

bool ABBAuroraComponent::read_baudrate_setting_central_(uint8_t baudcode, uint8_t serialline)
{
    return send_(this->address_, (uint8_t)85, baudcode, serialline, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
}

bool ABBAuroraComponent::read_system_info_central_(uint8_t var)
{
    return send_(this->address_, (uint8_t)101, var, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
}

bool ABBAuroraComponent::read_junctionbox_monitoring_central_(uint8_t cf, uint8_t rn, uint8_t njt, uint8_t jal, uint8_t jah)
{
    return send_(this->address_, (uint8_t)103, cf, rn, njt, jal, jah, (uint8_t)0);
}

bool ABBAuroraComponent::read_system_partnumber_central_()
{
    return send_(this->address_, (uint8_t)105, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
}

bool ABBAuroraComponent::read_system_serialnumber_central_()
{
    return send_(this->address_, (uint8_t)107, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);
}

bool ABBAuroraComponent::read_state_()
{
    State_.ReadState = send_(this->address_, (uint8_t)50, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    if (State_.ReadState == false)
    {
        receive_data_[0] = 255;
        receive_data_[1] = 255;
        receive_data_[2] = 255;
        receive_data_[3] = 255;
        receive_data_[4] = 255;
        receive_data_[5] = 255;
    }

    State_.TransmissionState = receive_data_[0];
    State_.GlobalState = receive_data_[1];
    State_.InverterState = receive_data_[2];
    State_.Channel1State = receive_data_[3];
    State_.Channel2State = receive_data_[4];
    State_.AlarmState = receive_data_[5];

    ESP_LOGV( TAG, "TransmissionState: %s", ABBAuroraComponent::transmission_state_text(State_.TransmissionState).c_str() );
    ESP_LOGV( TAG, "GlobalState: %s", ABBAuroraComponent::global_state_text(State_.GlobalState).c_str() );
    ESP_LOGV( TAG, "InverterState: %s", ABBAuroraComponent::inverter_state_text(State_.InverterState).c_str() );
    ESP_LOGV( TAG, "Channel1State: %s", ABBAuroraComponent::dcdc_state_text(State_.Channel1State).c_str() );
    ESP_LOGV( TAG, "Channel2State: %s", ABBAuroraComponent::dcdc_state_text(State_.Channel2State).c_str() );
    ESP_LOGV( TAG, "AlarmState: %s", ABBAuroraComponent::alarm_state_text(State_.AlarmState).c_str() );

    return State_.ReadState;
}

bool ABBAuroraComponent::read_version_()
{
    Version_.ReadState = send_(this->address_, (uint8_t)58, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0, (uint8_t)0);

    if(Version_.ReadState == false)
    {
        receive_data_[0] = 255;
        receive_data_[1] = 255;
    }

    Version_.TransmissionState = receive_data_[0];
    Version_.GlobalState = receive_data_[1];

    switch ((char)receive_data_[2])
    {
        case 'i': Version_.Par1 = std::string("Aurora 2 kW indoor"); break;
        case 'o': Version_.Par1 = std::string("Aurora 2 kW outdoor"); break;
        case 'r': Version_.Par1 = std::string("ABB TRIO 5.8kW outdoor"); break;
        case 'I': Version_.Par1 = std::string("Aurora 3.6 kW indoor"); break;
        case 'O': Version_.Par1 = std::string("Aurora 3.0 - 3.6 kW outdoor"); break;
        case '5': Version_.Par1 = std::string("Aurora 5.0 kW outdoor"); break;
        case '6': Version_.Par1 = std::string("Aurora 6 kW outdoor"); break;
        case 'P': Version_.Par1 = std::string("3 - phase interface (3G74)"); break;
        case 'C': Version_.Par1 = std::string("Aurora 50kW module"); break;
        case '4': Version_.Par1 = std::string("Aurora 4.2kW new"); break;
        case '3': Version_.Par1 = std::string("Aurora 3.6kW new"); break;
        case '2': Version_.Par1 = std::string("Aurora 3.3kW new"); break;
        case '1': Version_.Par1 = std::string("Aurora 3.0kW new"); break;
        case 'D': Version_.Par1 = std::string("Aurora 12.0kW"); break;
        case 'X': Version_.Par1 = std::string("Aurora 10.0kW"); break;
        default: ESP_LOGD(TAG, "Version_.par1 unknown [%d]", receive_data_[2] );
                 Version_.Par1 = std::string("Unknown");
    }
 
    switch ((char)receive_data_[3])
    {
        case 'A': Version_.Par2 = std::string("UL1741"); break;
        case 'B': Version_.Par2 = std::string("NETHERL"); break;
        case 'E': Version_.Par2 = std::string("VDE0126"); break;
        case 'S': Version_.Par2 = std::string("DR 1663 / 2000"); break;
        case 'I': Version_.Par2 = std::string("ENEL DK 5950"); break;
        case 'U': Version_.Par2 = std::string("UK G83"); break;
        case 'K': Version_.Par2 = std::string("AS 4777"); break;
        default: ESP_LOGD(TAG, "Version_.par2 unknown [%d]", receive_data_[3] );
                 Version_.Par2 = std::string("Unknown"); 
    }

    switch ((char)receive_data_[4])
    {
        case 'N': Version_.Par3 = std::string("Transformerless Version"); break;
        case 'T': Version_.Par3 = std::string("Transformer Version"); break;
        default: ESP_LOGD(TAG, "Version_.par3 unknown [%d]", receive_data_[4] );
                 Version_.Par3 = std::string("Unknown");
    }

    switch ((char)receive_data_[5])
    {
        case 'W': Version_.Par4 = std::string("Wind version"); break;
        case 'N': Version_.Par4 = std::string("PV version"); break;
        default: ESP_LOGD(TAG, "Version_.par4 unknown [%d]", receive_data_[5] );
                 Version_.Par4 = std::string("Unknown");
    }
    return Version_.ReadState;
};

std::string ABBAuroraComponent::alarm_state_text_(uint8_t id)
{
    switch (id)
    {
        case 0: return std::string("No Alarm");
        case 1: return std::string("Sun Low");
        case 2: return std::string("Input OC");
        case 3: return std::string("Input UV");
        case 4: return std::string("Input OV");
        case 5: return std::string("Sun Low");
        case 6: return std::string("No Parameters");
        case 7: return std::string("Bulk OV");
        case 8: return std::string("Comm.Error");
        case 9: return std::string("Output OC");
        case 10: return std::string("IGBT Sat");
        case 11: return std::string("Bulk UV");
        case 12: return std::string("Internal error");
        case 13: return std::string("Grid Fail");
        case 14: return std::string("Bulk Low");
        case 15: return std::string("Ramp Fail");
        case 16: return std::string("Dc / Dc Fail");
        case 17: return std::string("Wrong Mode");
        case 18: return std::string("Ground Fault");
        case 19: return std::string("Over Temp.");
        case 20: return std::string("Bulk Cap Fail");
        case 21: return std::string("Inverter Fail");
        case 22: return std::string("Start Timeout");
        case 23: return std::string("Ground Fault");
        case 24: return std::string("Degauss error");
        case 25: return std::string("Ileak sens.fail");
        case 26: return std::string("DcDc Fail");
        case 27: return std::string("Self Test Error 1");
        case 28: return std::string("Self Test Error 2");
        case 29: return std::string("Self Test Error 3");
        case 30: return std::string("Self Test Error 4");
        case 31: return std::string("DC inj error");
        case 32: return std::string("Grid OV");
        case 33: return std::string("Grid UV");
        case 34: return std::string("Grid OF");
        case 35: return std::string("Grid UF");
        case 36: return std::string("Z grid Hi");
        case 37: return std::string("Internal error");
        case 38: return std::string("Riso Low");
        case 39: return std::string("Vref Error");
        case 40: return std::string("Error Meas V");
        case 41: return std::string("Error Meas F");
        case 42: return std::string("Error Meas Z");
        case 43: return std::string("Error Meas Ileak");
        case 44: return std::string("Error Read V");
        case 45: return std::string("Error Read I");
        case 46: return std::string("Table fail");
        case 47: return std::string("Fan Fail");
        case 48: return std::string("UTH");
        case 49: return std::string("Interlock fail");
        case 50: return std::string("Remote Off");
        case 51: return std::string("Vout Avg errror");
        case 52: return std::string("Battery low");
        case 53: return std::string("Clk fail");
        case 54: return std::string("Input UC");
        case 55: return std::string("Zero Power");
        case 56: return std::string("Fan Stucked");
        case 57: return std::string("DC Switch Open");
        case 58: return std::string("Tras Switch Open");
        case 59: return std::string("AC Switch Open");
        case 60: return std::string("Bulk UV");
        case 61: return std::string("Autoexclusion");
        case 62: return std::string("Grid df / dt");
        case 63: return std::string("Den switch Open");
        case 64: return std::string("Jbox fail");
        default: ESP_LOGD(TAG, "Alarm Text unknown [%d]", id );
                 return std::string("Unknown");
    }
}

std::string ABBAuroraComponent::transmission_state_text_(uint8_t id)
{
    switch (id)
    {
        case 0:  return std::string("Everything is OK.");
        case 51: return std::string("Command is not implemented");
        case 52: return std::string("Variable does not exist");
        case 53: return std::string("Variable value is out of range");
        case 54: return std::string("EEprom not accessible");
        case 55: return std::string("Not Toggled Service Mode");
        case 56: return std::string("Can not send the command to internal micro");
        case 57: return std::string("Command not Executed");
        case 58: return std::string("The variable is not available, retry");
        default: ESP_LOGD(TAG, "Transmission Text unknown [%d]", id );
                 return std::string("Unknown");
    }
}

std::string ABBAuroraComponent::global_state_text_(uint8_t id)
{
    switch (id)
    {
        case 0: return std::string("Sending Parameters");
        case 1: return std::string("Wait Sun / Grid");
        case 2: return std::string("Checking Grid");
        case 3: return std::string("Measuring Riso");
        case 4: return std::string("DcDc Start");
        case 5: return std::string("Inverter Start");
        case 6: return std::string("Run");
        case 7: return std::string("Recovery");
        case 8: return std::string("Pausev");
        case 9: return std::string("Ground Fault");
        case 10: return std::string("OTH Fault");
        case 11: return std::string("Address Setting");
        case 12: return std::string("Self Test");
        case 13: return std::string("Self Test Fail");
        case 14: return std::string("Sensor Test + Meas.Riso");
        case 15: return std::string("Leak Fault");
        case 16: return std::string("Waiting for manual reset");
        case 17: return std::string("Internal Error E026");
        case 18: return std::string("Internal Error E027");
        case 19: return std::string("Internal Error E028");
        case 20: return std::string("Internal Error E029");
        case 21: return std::string("Internal Error E030");
        case 22: return std::string("Sending Wind Table");
        case 23: return std::string("Failed Sending table");
        case 24: return std::string("UTH Fault");
        case 25: return std::string("Remote OFF");
        case 26: return std::string("Interlock Fail");
        case 27: return std::string("Executing Autotest");
        case 30: return std::string("Waiting Sun");
        case 31: return std::string("Temperature Fault");
        case 32: return std::string("Fan Staucked");
        case 33: return std::string("Int.Com.Fault");
        case 34: return std::string("Slave Insertion");
        case 35: return std::string("DC Switch Open");
        case 36: return std::string("TRAS Switch Open");
        case 37: return std::string("MASTER Exclusion");
        case 38: return std::string("Auto Exclusion");
        case 98: return std::string("Erasing Internal EEprom");
        case 99: return std::string("Erasing External EEprom");
        case 100: return std::string("Counting EEprom");
        case 101: return std::string("Freeze");
        default: ESP_LOGD(TAG, "GlobalState Text unknown [%d]", id );
                 return std::string("Unknown");
    }
}

std::string ABBAuroraComponent::dcdc_state_text_(uint8_t id)
{
    switch (id)
    {
        case 0: return std::string("DcDc OFF");
        case 1: return std::string("Ramp Start");
        case 2: return std::string("MPPT");
        case 3: return std::string("Not Used");
        case 4: return std::string("Input OC");
        case 5: return std::string("Input UV");
        case 6: return std::string("Input OV");
        case 7: return std::string("Input Low");
        case 8: return std::string("No Parameters");
        case 9: return std::string("Bulk OV");
        case 10: return std::string("Communication Error");
        case 11: return std::string("Ramp Fail");
        case 12: return std::string("Internal Error");
        case 13: return std::string("Input mode Error");
        case 14: return std::string("Ground Fault");
        case 15: return std::string("Inverter Fail");
        case 16: return std::string("DcDc IGBT Sat");
        case 17: return std::string("DcDc ILEAK Fail");
        case 18: return std::string("DcDc Grid Fail");
        case 19: return std::string("DcDc Comm.Error");
        default: ESP_LOGD(TAG, "DcDcState Text unknown [%d]", id );
                 return std::string("Unknown");
    }
}

std::string ABBAuroraComponent::inverter_state_text_(uint8_t id)
{
    switch (id)
    {
        case 0: return std::string("Stand By");
        case 1: return std::string("Checking Grid");
        case 2: return std::string("Run");
        case 3: return std::string("Bulk OV");
        case 4: return std::string("Out OC");
        case 5: return std::string("IGBT Sat");
        case 6: return std::string("Bulk UV");
        case 7: return std::string("Degauss Error");
        case 8: return std::string("No Parameters");
        case 9: return std::string("Bulk Low");
        case 10: return std::string("Grid OV");
        case 11: return std::string("Communication Error");
        case 12: return std::string("Degaussing");
        case 13: return std::string("Starting");
        case 14: return std::string("Bulk Cap Fail");
        case 15: return std::string("Leak Fail");
        case 16: return std::string("DcDc Fail");
        case 17: return std::string("Ileak Sensor Fail");
        case 18: return std::string("SelfTest: relay inverter");
        case 19: return std::string("SelfTest : wait for sensor test");
        case 20: return std::string("SelfTest : test relay DcDc + sensor");
        case 21: return std::string("SelfTest : relay inverter fail");
        case 22: return std::string("SelfTest timeout fail");
        case 23: return std::string("SelfTest : relay DcDc fail");
        case 24: return std::string("Self Test 1");
        case 25: return std::string("Waiting self test start");
        case 26: return std::string("Dc Injection");
        case 27: return std::string("Self Test 2");
        case 28: return std::string("Self Test 3");
        case 29: return std::string("Self Test 4");
        case 30: return std::string("Internal Error");
        case 31: return std::string("Internal Error");
        case 40: return std::string("Forbidden State");
        case 41: return std::string("Input UC");
        case 42: return std::string("Zero Power");
        case 43: return std::string("Grid Not Present");
        case 44: return std::string("Waiting Start");
        case 45: return std::string("MPPT");
        case 46: return std::string("Grid Fail");
        case 47: return std::string("Input OC");
        default: ESP_LOGD(TAG, "InverterState Text unknown [%d]", id );
                 return std::string("Unknown");       
    }
}

} // abbaurora namespace
} // esphome namespace

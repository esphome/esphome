#include "ads1220.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ads1220 {

static const char *const TAG = "ADS1220";
static const uint8_t ADS1220_REGISTER_CONVERSION = 0x00;
static const uint8_t ADS1220_REGISTER_CONFIG = 0x01;

static const uint8_t ADS1220_DATA_RATE_860_SPS = 0b111;  // 3300_SPS for ADS1015

//ADS1220 SPI commands
static constexpr uint8_t ADS1220_RESET       {0x06};
static constexpr uint8_t ADS1220_START       {0x08};    //Send the START/SYNC command (08h) to start converting in continuous conversion mode
static constexpr uint8_t ADS1220_PWRDOWN     {0x02};
static constexpr uint8_t ADS1220_RDATA       {0x10};
static constexpr uint8_t ADS1220_WREG        {0x40};    // write register
static constexpr uint8_t ADS1220_RREG        {0x20};    // read register

/* registers */
static constexpr uint8_t ADS1220_CONF_REG_0  {0x00};
static constexpr uint8_t ADS1220_CONF_REG_1  {0x01};
static constexpr uint8_t ADS1220_CONF_REG_2  {0x02};
static constexpr uint8_t ADS1220_CONF_REG_3  {0x03};


void ADS1220Component::setup() {
	ESP_LOGCONFIG(TAG, "Setting up ADS1220...");
	uint16_t value;
	uint8_t value_0;
	uint8_t value_1;
	uint8_t value_2;
	uint8_t value_3;

    this->spi_setup();
    //pinMode(csPin, OUTPUT);
    //digitalWrite(csPin, HIGH);
    pinMode(drdyPin, INPUT);
    
    //vRef = 2.048;
    vRef = 3.300;
    gain = 1;
    vfsr = vRef/gain;
    refMeasurement = false;
    convMode = ADS1220_SINGLE_SHOT;
    reset();
    start();
    uint8_t ctrlVal = 0;
    bypassPGA(true); // just a test if the ADS1220 is connected
    value_0 = this->readRegister(ADS1220_CONF_REG_0);
    value_1 = this->readRegister(ADS1220_CONF_REG_1);
    value_2 = this->readRegister(ADS1220_CONF_REG_2);
    value_3 = this->readRegister(ADS1220_CONF_REG_3);
    ctrlVal = value_0;
    ctrlVal = ctrlVal & 0x01;
    bypassPGA(false);
    if (!ctrlVal) {
        ESP_LOGE(TAG, "Communication with ADS1220 failed!");
        this->mark_failed();
        return;
    }

	ESP_LOGCONFIG(TAG, "Configuring ADS1220...");
/*
    // Setup operation mode
    setOperatingMode(ADS1220_TURBO_MODE);
    // Setup Gain
    setGain(ADS1220_GAIN_1);
    // Set mode
    if (this->continuous_mode_) {
        // Set continuous mode
        setConversionMode(ADS1220_CONTINUOUS);
    } else {
        // Set singleshot mode
        setConversionMode(ADS1220_SINGLE_SHOT);
    }
    // Set data rate - 860 samples per second (we're in singleshot mode)
    setVRefSource(ADS1220_VREF_AVDD_AVSS);
    setDrdyMode(ADS1220_DRDY_ONLY);
    setDataRate(ADS1220_DR_LVL_4);
    */
}

void ADS1220Component::dump_config() {
    ESP_LOGCONFIG(TAG, "Setting up ADS1220...");
    //LOG_I2C_DEVICE(this);
    if (this->is_failed()) {
        ESP_LOGE(TAG, "Communication with ADS1220 failed!");
    }
    
    for (auto *sensor : this->sensors_) {
        LOG_SENSOR("  ", "Sensor", sensor);
        ESP_LOGCONFIG(TAG, "    Multiplexer: %u", sensor->get_multiplexer());
        ESP_LOGCONFIG(TAG, "    Gain: %u", sensor->get_gain());
    }
}





/***********************************
 * 
 * Request measurement
 * 
 **********************************/
float ADS1220Component::request_measurement(ADS1220Sensor *sensor) {
    float resultInMV = 0.0;
    float resultInVoltage = 0.000;
    
    uint16_t config = 0;

    // Setup gain
    config = sensor->get_gain();
    //ESP_LOGI(TAG, "ADS1220 gain: 0x%02x", (ads1220Gain)config);
    if (prev_gain != config) {
        setGain((ads1220Gain)config);
        prev_gain = (ads1220Gain)config;
    }
	// Setup datarate
    config = sensor->get_datarate();
    //ESP_LOGI(TAG, "ADS1220 datarate: 0x%02x", (ads1220DataRate)config);
    if (prev_datarate != config) {
        setDataRate((ads1220DataRate)config);
        prev_datarate = (ads1220DataRate)config;
    }
	// Setup operating mode
    config = sensor->get_operating_mode();
    //ESP_LOGI(TAG, "ADS1220 op mode: 0x%02x", (ads1220OpMode)config);
    if (prev_op_mode != config) {
        setOperatingMode((ads1220OpMode)config);
        prev_op_mode = (ads1220OpMode)config;
    }
	// Setup conversion mode
    config = sensor->get_conversion_mode();
    //ESP_LOGI(TAG, "ADS1220 conv mode: 0x%02x", (ads1220ConvMode)config);
    if (prev_conv_mode != config) {
        setConversionMode((ads1220ConvMode)config);
        prev_conv_mode = (ads1220ConvMode)config;
    }
	// Setup voltage reference
    config = sensor->get_vref_source();
    //ESP_LOGI(TAG, "ADS1220 vref: 0x%02x", (ads1220VRef)config);
    if (prev_vref_source != config) {
    	setVRefSource((ads1220VRef)config);
    	prev_vref_source = (ads1220VRef)config;
    }
	// Setup dataready pin mode
    config = sensor->get_drdy_mode();
    //ESP_LOGI(TAG, "ADS1220 drdy: 0x%02x", (ads1220DrdyMode)config);
    if (prev_drdy_mode != config) {
    	setDrdyMode((ads1220DrdyMode)config);
    	prev_drdy_mode = (ads1220DrdyMode)config;
    }

	// Setup temperature sensor
    config = sensor->get_temp_sensor_mode();
    ESP_LOGI(TAG, "ADS1220 temp sensor mode: 0x%02x", config);
    if (prev_temp_sensor != config) {
        //enableTemperatureSensor(config);
        prev_temp_sensor = config;
    }
	// Setup burnout current sources
    config = sensor->get_burnout_current_sources();
    ESP_LOGI(TAG, "ADS1220 fault test mode: 0x%02x", config);
    if (prev_burnout_current_sources != config) {
        //enableBurnOutCurrentSources(config);
        prev_burnout_current_sources = config;
    }



	// Setup multiplexer
    config = sensor->get_multiplexer();
    //ESP_LOGI(TAG, "ADS1220 mux: 0x%02x", (ads1220Multiplexer)config);
    if (prev_multiplexer != config) {
        setCompareChannels((ads1220Multiplexer)config);
        prev_multiplexer = (ads1220Multiplexer)config;
    }

    resultInMV = getVoltage_mV();
    resultInVoltage = resultInMV / 1000.0;
    
    this->status_clear_warning();
    return resultInVoltage;
}




/***********************************
 * 
 * Set configuration
 * 
 **********************************/
/* Configuration Register 0 settings */
void ADS1220Component::setCompareChannels(ads1220Multiplexer mux){
    if((mux == ADS1220_MULTIPLEXER_REFPX_REFNX_4) || (mux == ADS1220_MULTIPLEXER_AVDD_M_AVSS_4)){
        gain = 1;    // under these conditions gain is one by definition
        refMeasurement = true;
    }
    else{            // otherwise read gain from register
        regValue = spi_device_register_0_buffered;//readRegister(ADS1220_CONF_REG_0);
        regValue = regValue & 0x0E;
        regValue = regValue>>1;
        gain = 1 << regValue;
        refMeasurement = false;
    }
    regValue = spi_device_register_0_buffered;//readRegister(ADS1220_CONF_REG_0);
    regValue &= ~0xF1;
    regValue |= mux;
    regValue |= !(doNotBypassPgaIfPossible & 0x01);

    //ESP_LOGI(TAG, "Write compare channels in ADS1220 to value %u", regValue);
    writeRegister(ADS1220_CONF_REG_0, regValue);
    if((mux >= 0x80) && (mux <=0xD0)){
        if(gain > 4){
            gain = 4;           // max gain is 4 if single-ended input is chosen or PGA is bypassed
        }
        forcedBypassPGA();
    }
}

void ADS1220Component::setGain(ads1220Gain enumGain) {
    regValue = readRegister(ADS1220_CONF_REG_0);
    ads1220Multiplexer mux = (ads1220Multiplexer)(regValue & 0xF0);
    regValue &= ~0x0E;
    regValue |= enumGain;
    writeRegister(ADS1220_CONF_REG_0, regValue);

    gain = 1<<(enumGain>>1);
    if((mux >= 0x80) && (mux <=0xD0)){
        if(gain > 4){
            gain = 4;   // max gain is 4 if single-ended input is chosen or PGA is bypassed
        }
        forcedBypassPGA();
    }
}

uint8_t ADS1220Component::getGainFactor(){
    return gain;
}

void ADS1220Component::bypassPGA(bool bypass){
    regValue = readRegister(ADS1220_CONF_REG_0);
    regValue &= ~0x01;
    regValue |= bypass;
    doNotBypassPgaIfPossible = !(bypass & 0x01);
    writeRegister(ADS1220_CONF_REG_0, regValue);
}

bool ADS1220Component::isPGABypassed(){
    regValue = readRegister(ADS1220_CONF_REG_0);
    return regValue & 0x01;
}

/* Configuration Register 1 settings */
void ADS1220Component::setDataRate(ads1220DataRate rate){
    regValue = readRegister(ADS1220_CONF_REG_1);
    regValue &= ~0xE0;
    regValue |= rate;
    writeRegister(ADS1220_CONF_REG_1, regValue);
}

void ADS1220Component::setOperatingMode(ads1220OpMode mode){
    regValue = readRegister(ADS1220_CONF_REG_1);
    regValue &= ~0x18;
    regValue |= mode;
    writeRegister(ADS1220_CONF_REG_1, regValue);
}

void ADS1220Component::setConversionMode(ads1220ConvMode mode){
    convMode = mode;
    regValue = readRegister(ADS1220_CONF_REG_1);
    regValue &= ~0x04;
    regValue |= mode;
    writeRegister(ADS1220_CONF_REG_1, regValue);
}

void ADS1220Component::enableTemperatureSensor(bool enable){
    regValue = readRegister(ADS1220_CONF_REG_1);
    if(enable){
        regValue |= 0x02;
    }
    else{
        regValue &= ~0x02;
    }
    writeRegister(ADS1220_CONF_REG_1, regValue);
}

void ADS1220Component::enableBurnOutCurrentSources(bool enable){
    regValue = readRegister(ADS1220_CONF_REG_1);
    if(enable){
        regValue |= 0x01;
    }
    else{
        regValue &= ~0x01;
    }
    writeRegister(ADS1220_CONF_REG_1, regValue);
}


/* Configuration Register 2 settings */
void ADS1220Component::setVRefSource(ads1220VRef vRefSource){
    regValue = readRegister(ADS1220_CONF_REG_2);
    regValue &= ~0xC0;
    regValue |= vRefSource;
    writeRegister(ADS1220_CONF_REG_2, regValue);
}

void ADS1220Component::setFIRFilter(ads1220FIR fir){
    regValue = readRegister(ADS1220_CONF_REG_2);
    regValue &= ~0x30;
    regValue |= fir;
    writeRegister(ADS1220_CONF_REG_2, regValue);
}

void ADS1220Component::setLowSidePowerSwitch(ads1220PSW psw){
    regValue = readRegister(ADS1220_CONF_REG_2);
    regValue &= ~0x08;
    regValue |= psw;
    writeRegister(ADS1220_CONF_REG_2, regValue);
}

void ADS1220Component::setIdacCurrent(ads1220IdacCurrent current){
    regValue = readRegister(ADS1220_CONF_REG_2);
    regValue &= ~0x07;
    regValue |= current;
    writeRegister(ADS1220_CONF_REG_2, regValue);
    //delayMicroseconds(200);
}

/* Configuration Register 3 settings */

void ADS1220Component::setIdac1Routing(ads1220IdacRouting route){
    regValue = readRegister(ADS1220_CONF_REG_3);
    regValue &= ~0xE0;
    regValue |= (route<<5);
    writeRegister(ADS1220_CONF_REG_3, regValue);
}

void ADS1220Component::setIdac2Routing(ads1220IdacRouting route){
    regValue = readRegister(ADS1220_CONF_REG_3);
    regValue &= ~0x1C;
    regValue |= (route<<2);
    writeRegister(ADS1220_CONF_REG_3, regValue);
}

void ADS1220Component::setDrdyMode(ads1220DrdyMode mode){
    regValue = readRegister(ADS1220_CONF_REG_3);
    regValue &= ~0x02;
    regValue |= mode;
    writeRegister(ADS1220_CONF_REG_3, regValue);
}

/* Other settings */
void ADS1220Component::setVRefValue(float refVal){
    vRef = refVal;
}

float ADS1220Component::getVRef_V(){
    return vRef;
}

void ADS1220Component::setAvddAvssAsVrefAndCalibrate(){
    float avssVoltage = 0.0;
    setVRefSource(ADS1220_VREF_AVDD_AVSS);
    setCompareChannels(ADS1220_MULTIPLEXER_AVDD_M_AVSS_4);
    for(int i = 0; i<10; i++){
        avssVoltage += getVoltage_mV();
    }
    vRef = avssVoltage * 4.0 / 10000.0;
}

void ADS1220Component::setRefp0Refn0AsVefAndCalibrate(){
    float ref0Voltage = 0.0;
    setVRefSource(ADS1220_VREF_REFP0_REFN0);
    setCompareChannels(ADS1220_MULTIPLEXER_REFPX_REFNX_4);
    for(int i = 0; i<10; i++){
        ref0Voltage += getVoltage_mV();
    }
    vRef = ref0Voltage * 4.0 / 10000.0;
}

void ADS1220Component::setRefp1Refn1AsVefAndCalibrate(){
    float ref1Voltage = 0.0;
    setVRefSource(ADS1220_VREF_REFP1_REFN1);
    setCompareChannels(ADS1220_MULTIPLEXER_REFPX_REFNX_4);
    for(int i = 0; i<10; i++){
        ref1Voltage += getVoltage_mV();
    }
    vRef = ref1Voltage * 4.0 / 10000.0;
}

void ADS1220Component::setIntVRef(){
    setVRefSource(ADS1220_VREF_INT);
    vRef = 2.048;
}


/* Results */
float ADS1220Component::getVoltage_mV(){
    int32_t rawData = getData();
    float resultInMV = 0.0;
    if(refMeasurement){
        resultInMV = (rawData / ADS1220_RANGE) * 2.048 * 1000.0 / (gain * 1.0);
    }
    else{
        resultInMV = (rawData / ADS1220_RANGE) * vRef * 1000.0 / (gain * 1.0);
    }
    return (float)((rawData*vfsr*1000)/ADS1220_RANGE);
    //return resultInMV;
}

float ADS1220Component::getVoltage_muV(){
    return getVoltage_mV() * 1000.0;
}

int32_t ADS1220Component::getRawData(){
    return getData();
}

float ADS1220Component::getTemperature(){
    enableTemperatureSensor(true);
    uint32_t rawResult = readResult();
    enableTemperatureSensor(false);

    uint16_t result = static_cast<uint16_t>(rawResult >> 18);
    if(result>>13){
        result = ~(result-1) & 0x3777;
        return result * (-0.03125);
    }

    return result * 0.03125;
}

/************************************************
    private functions
*************************************************/

void ADS1220Component::forcedBypassPGA(){
    regValue = spi_device_register_0_buffered;//readRegister(ADS1220_CONF_REG_0);
    if (!(regValue & 0x01))
    {
        regValue |= 0x01;
        //ESP_LOGI(TAG, "Write forcedBypassPGA in ADS1220 to value %u", regValue);
        writeRegister(ADS1220_CONF_REG_0, regValue);
    }
}

int32_t ADS1220Component::getData(){
    uint32_t rawResult = readResult();
    int32_t result = (static_cast<int32_t>(rawResult)) >> 8;

    return result;
}

uint32_t ADS1220Component::readResult()
{
    uint8_t data[4] = { 0x00 };
    uint32_t rawResult = 0;

    //ESP_LOGI(TAG, "Read result from ADS1220!");
    //delay(1);

    if(convMode == ADS1220_SINGLE_SHOT){
        //ESP_LOGI(TAG, "Start conversion!");
        //delay(1);
        start();
    }
    while(digitalRead(drdyPin)) {}

    //ESP_LOGI(TAG, "Store result to memory!");
    //delay(1);
    
    this->enable();
    delay(1);
    
    data[0] = this->read_byte();
    data[1] = this->read_byte();
    data[2] = this->read_byte();

    this->disable();
    delay(1);

    rawResult = data[0];
    rawResult = (rawResult << 8) | data[1];
    rawResult = (rawResult << 8) | data[2];
    rawResult = (rawResult << 8);

    return rawResult;
}













void ADS1220Component::reset() {
    send_command(ADS1220_RESET);
}

void ADS1220Component::start() {
    send_command(ADS1220_START);
}

uint8_t ADS1220Component::readRegister(uint8_t reg)
{
    uint8_t data[4] = { 0x00 };;

    this->enable();
    delay(1);

    this->write_byte(ADS1220_RREG | (reg<<2));

    data[0] = this->read_byte();
    data[1] = this->read_byte();
    data[2] = this->read_byte();

    this->disable();
    delay(1);

    return data[0];
}

void ADS1220Component::writeRegister(uint8_t reg, uint8_t val)
{
    if (reg == ADS1220_CONF_REG_0)
        spi_device_register_0_buffered = val;
    if (reg == ADS1220_CONF_REG_1)
        spi_device_register_1_buffered = val;
    if (reg == ADS1220_CONF_REG_2)
        spi_device_register_2_buffered = val;
    if (reg == ADS1220_CONF_REG_3)
        spi_device_register_3_buffered = val;

    this->enable();
    delay(1);

    this->write_byte(ADS1220_WREG | (reg<<2));
    this->write_byte(val);

    this->disable();
    delay(1);

}

void ADS1220Component::send_command(uint8_t cmd)
{
    //ESP_LOGI(TAG, "Send command %u to ADS1220", cmd);
    
    this->enable();
    delay(1);
    
    this->write_byte(cmd);

    this->disable();
    delay(1);
}





float ADS1220Sensor::sample() {
    return this->parent_->request_measurement(this);
}

void ADS1220Sensor::update() {
  float v = this->parent_->request_measurement(this);
  if (!std::isnan(v)) {
    ESP_LOGD(TAG, "'%s': Got Voltage=%fV", this->get_name().c_str(), v);
    this->publish_state(v);
  }
}

}  // namespace ads1220
}  // namespace esphome
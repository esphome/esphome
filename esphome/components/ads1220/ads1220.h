#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include <vector>

namespace esphome {
namespace ads1220 {

typedef enum ADS1220Multiplexer {
    ADS1220_MULTIPLEXER_P0_N1 = 0x00,   //default
    ADS1220_MULTIPLEXER_P0_N2 = 0x10,
    ADS1220_MULTIPLEXER_P0_N3 = 0x20,
    ADS1220_MULTIPLEXER_P1_N2 = 0x30,
    ADS1220_MULTIPLEXER_P1_N3 = 0x40,
    ADS1220_MULTIPLEXER_P2_N3 = 0x50,
    ADS1220_MULTIPLEXER_P1_N0 = 0x60,
    ADS1220_MULTIPLEXER_P3_N2 = 0x70,
    ADS1220_MULTIPLEXER_P0_NG = 0x80,
    ADS1220_MULTIPLEXER_P1_NG = 0x90,
    ADS1220_MULTIPLEXER_P2_NG = 0xA0,
    ADS1220_MULTIPLEXER_P3_NG = 0xB0,
    ADS1220_MULTIPLEXER_REFPX_REFNX_4 = 0xC0,
    ADS1220_MULTIPLEXER_AVDD_M_AVSS_4 = 0xD0,
    ADS1220_MULTIPLEXER_AVDD_P_AVSS_2 = 0xE0
} ads1220Multiplexer;

typedef enum ADS1220Gain {
    ADS1220_GAIN_1   = 0x00,   //default
    ADS1220_GAIN_2   = 0x02,
    ADS1220_GAIN_4   = 0x04,
    ADS1220_GAIN_8   = 0x06,
    ADS1220_GAIN_16  = 0x08,
    ADS1220_GAIN_32  = 0x0A,
    ADS1220_GAIN_64  = 0x0C,
    ADS1220_GAIN_128 = 0x0E
} ads1220Gain;

typedef enum ADS1220DataRate {
    ADS1220_DR_LVL_0 = 0x00,   // default
    ADS1220_DR_LVL_1 = 0x20,
    ADS1220_DR_LVL_2 = 0x40,
    ADS1220_DR_LVL_3 = 0x60,
    ADS1220_DR_LVL_4 = 0x80,
    ADS1220_DR_LVL_5 = 0xA0,
    ADS1220_DR_LVL_6 = 0xC0
} ads1220DataRate;

typedef enum ADS1220OpMode {
    ADS1220_NORMAL_MODE     = 0x00,  // default
    ADS1220_DUTY_CYCLE_MODE = 0x08,
    ADS1220_TURBO_MODE      = 0x10
} ads1220OpMode;

typedef enum ADS1220ConvMode {
    ADS1220_SINGLE_SHOT     = 0x00,  // default
    ADS1220_CONTINUOUS      = 0x04
} ads1220ConvMode;

typedef enum ADS1220VRef {
    ADS1220_VREF_INT            = 0x00,  // default
    ADS1220_VREF_REFP0_REFN0    = 0x40,
    ADS1220_VREF_REFP1_REFN1    = 0x80,
    ADS1220_VREF_AVDD_AVSS      = 0xC0
} ads1220VRef;

typedef enum ADS1220_FIR{
    ADS1220_NONE        = 0x00,   // default
    ADS1220_50HZ_60HZ   = 0x10,
    ADS1220_50HZ        = 0x20,
    ADS1220_60HZ        = 0x30
} ads1220FIR;

typedef enum ADS1220PSW {
    ADS1220_ALWAYS_OPEN = 0x00,  // default
    ADS1220_SWITCH      = 0x08
} ads1220PSW;

typedef enum ADS1220_IDAC_CURRENT {
    ADS1220_IDAC_OFF        = 0x00,  // defaulr
    ADS1220_IDAC_10_MU_A    = 0x01,
    ADS1220_IDAC_50_MU_A    = 0x02,
    ADS1220_IDAC_100_MU_A   = 0x03,
    ADS1220_IDAC_250_MU_A   = 0x04,
    ADS1220_IDAC_500_MU_A   = 0x05,
    ADS1220_IDAC_1000_MU_A  = 0x06,
    ADS1220_IDAC_1500_MU_A  = 0x07
} ads1220IdacCurrent;

typedef enum ADS1220IdacRouting {
    ADS1220_IDAC_NONE       = 0x00,  // default
    ADS1220_IDAC_AIN0_REFP1 = 0x01,
    ADS1220_IDAC_AIN1       = 0x02,
    ADS1220_IDAC_AIN2       = 0x03,
    ADS1220_IDAC_AIN3_REFN1 = 0x04,
    ADS1220_IDAC_REFP0      = 0x05,
    ADS1220_IDAC_REFN0      = 0x06,
} ads1220IdacRouting;

typedef enum ADS1220DrdyMode {
    ADS1220_DRDY_ONLY = 0x00,   // default
    ADS1220_DOUT_DRDY = 0x02
} ads1220DrdyMode;

typedef enum ADS1220Resolution {
    ADS1220_24_BITS = 24,
} ads1220Resolution;

class ADS1220Sensor;

class ADS1220Component : public Component, public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
    public:
        void register_sensor(ADS1220Sensor *obj) { this->sensors_.push_back(obj); }
        /// Set up the internal sensor array.
        void setup() override;
        void dump_config() override;
        /// HARDWARE_LATE setup priority
        float get_setup_priority() const override { return setup_priority::DATA; }
        void set_continuous_mode(bool continuous_mode) { continuous_mode_ = continuous_mode; }
    
        /// Helper method to request a measurement from a sensor.
        float request_measurement(ADS1220Sensor *sensor);

     protected:
        std::vector<ADS1220Sensor *> sensors_;
        uint16_t prev_config_{0};
        bool continuous_mode_ = false;

        uint8_t csPin;
        uint8_t drdyPin = 0;
        uint8_t regValue;
        float vRef;
        uint8_t gain;
        float vfsr;
        bool refMeasurement;
        ads1220ConvMode convMode;
        bool doNotBypassPgaIfPossible = false;
        InternalGPIOPin *drdy_pin{nullptr};

        uint8_t spi_device_register_0_buffered;
        uint8_t spi_device_register_1_buffered;
        uint8_t spi_device_register_2_buffered;
        uint8_t spi_device_register_3_buffered;
        float channel_0_voltage;
        float channel_1_voltage;
        float channel_2_voltage;
        float channel_3_voltage;
        static constexpr float ADS1220_RANGE {8388607.0}; // = 2^23 - 1 as float

        /* Configuration Register 0 settings */
        void setCompareChannels(ads1220Multiplexer mux);
        void setGain(ads1220Gain gain);
        uint8_t getGainFactor();
        void bypassPGA(bool bypass);
        bool isPGABypassed();

        /* Configuration Register 1 settings */
        void setDataRate(ads1220DataRate rate);
        void setOperatingMode(ads1220OpMode mode);
        void setConversionMode(ads1220ConvMode mode);
        void enableTemperatureSensor(bool enable);
        void enableBurnOutCurrentSources(bool enable);

        /* Configuration Register 2 settings */
        void setVRefSource(ads1220VRef vRefSource);
        void setFIRFilter(ads1220FIR fir);
        void setLowSidePowerSwitch(ads1220PSW psw);
        void setIdacCurrent(ads1220IdacCurrent current);

        /* Configuration Register 3 settings */
        void setIdac1Routing(ads1220IdacRouting route);
        void setIdac2Routing(ads1220IdacRouting route);
        void setDrdyMode(ads1220DrdyMode mode);

        /* Other settings */
        void setVRefValue_V(float refVal);
        float getVRef_V();
        void setAvddAvssAsVrefAndCalibrate();
        void setRefp0Refn0AsVefAndCalibrate();
        void setRefp1Refn1AsVefAndCalibrate();
        void setIntVRef();

        /* Results */
        float getVoltage_mV();
        float getVoltage_muV();
        int32_t getRawData();
        float getTemperature();

        void reset();
        void start();

        void forcedBypassPGA();
        int32_t getData();
        uint32_t readResult();
        uint8_t readRegister(uint8_t reg);
        void writeRegister(uint8_t reg, uint8_t val);
        void send_command(uint8_t cmd);
    
};

/// Internal holder class that is in instance of Sensor so that the hub can create individual sensors.
class ADS1220Sensor : public sensor::Sensor, public PollingComponent, public voltage_sampler::VoltageSampler {
    public:
        ADS1220Sensor(ADS1220Component *parent) : parent_(parent) {}
        void update() override;
        void set_multiplexer(ADS1220Multiplexer multiplexer) { multiplexer_ = multiplexer; }
        void set_gain(ADS1220Gain gain) { gain_ = gain; }
        void set_resolution(ADS1220Resolution resolution) { resolution_ = resolution; }
        float sample() override;
        uint8_t get_multiplexer() const { return multiplexer_; }
        uint8_t get_gain() const { return gain_; }
        uint8_t get_resolution() const { return resolution_; }
    
    protected:
        ADS1220Component *parent_;
        ADS1220Multiplexer multiplexer_;
        ADS1220Gain gain_;
        ADS1220Resolution resolution_;
};

}  // namespace ads1220
}  // namespace esphome
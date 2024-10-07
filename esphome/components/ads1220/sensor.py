import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_GAIN,
    CONF_MULTIPLEXER,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
    CONF_TYPE,
    CONF_ID,
)
from . import ads1220_ns, ADS1220Component

CODEOWNERS = ["@miikasyvanen"]
DEPENDENCIES = ["ads1220"]


ADS1220Multiplexer = ads1220_ns.enum("ADS1220Multiplexer")
MUX = {
    "A0_A1": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_N1,
    "A0_A2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_N2,
    "A0_A3": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_N3,
    "A1_A2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_N2,
    "A1_A3": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_N3,
    "A2_A3": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P2_N3,
    "A1_A0": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_N0,
    "A3_A2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P3_N2,
    "A0_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_NG,
    "A1_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_NG,
    "A2_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P2_NG,
    "A3_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P3_NG,
    "REFPX_REFNX_4": ADS1220Multiplexer.ADS1220_MULTIPLEXER_REFPX_REFNX_4,
    "AVDD_M_AVSS_4": ADS1220Multiplexer.ADS1220_MULTIPLEXER_AVDD_M_AVSS_4,
    "AVDD_P_AVSS_2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_AVDD_P_AVSS_2,
}

ADS1220Gain = ads1220_ns.enum("ADS1220Gain")
GAIN = {
    "1": ADS1220Gain.ADS1220_GAIN_1,
    "2": ADS1220Gain.ADS1220_GAIN_2,
    "4": ADS1220Gain.ADS1220_GAIN_4,
    "8": ADS1220Gain.ADS1220_GAIN_8,
    "16": ADS1220Gain.ADS1220_GAIN_16,
    "32": ADS1220Gain.ADS1220_GAIN_32,
    "64": ADS1220Gain.ADS1220_GAIN_64,
    "128": ADS1220Gain.ADS1220_GAIN_128,
}

ADS1220DataRate = ads1220_ns.enum("ADS1220DataRate")
DATARATE = {
    "DR_LVL_0": ADS1220DataRate.ADS1220_DR_LVL_0,
    "DR_LVL_1": ADS1220DataRate.ADS1220_DR_LVL_1,
    "DR_LVL_2": ADS1220DataRate.ADS1220_DR_LVL_2,
    "DR_LVL_3": ADS1220DataRate.ADS1220_DR_LVL_3,
    "DR_LVL_4": ADS1220DataRate.ADS1220_DR_LVL_4,
    "DR_LVL_5": ADS1220DataRate.ADS1220_DR_LVL_5,
    "DR_LVL_6": ADS1220DataRate.ADS1220_DR_LVL_6,
}

ADS1220OpMode = ads1220_ns.enum("ADS1220OpMode")
OP_MODE = {
    "NORMAL": ADS1220OpMode.ADS1220_NORMAL_MODE,
    "DUTY_CYCLE": ADS1220OpMode.ADS1220_DUTY_CYCLE_MODE,
    "TURBO": ADS1220OpMode.ADS1220_TURBO_MODE,
}

ADS1220ConvMode = ads1220_ns.enum("ADS1220ConvMode")
CONV_MODE = {
	"SINGLE_SHOT": ADS1220ConvMode.ADS1220_SINGLE_SHOT,
	"CONTINUOUS": ADS1220ConvMode.ADS1220_CONTINUOUS,
}

ADS1220VRef = ads1220_ns.enum("ADS1220VRef")
VREF = {
	"INTERNAL": ADS1220VRef.ADS1220_VREF_INT,
    "REFP0_REFN0": ADS1220VRef.ADS1220_VREF_REFP0_REFN0,
    "REFP1_REFN1": ADS1220VRef.ADS1220_VREF_REFP1_REFN1,
    "AVDD_AVSS": ADS1220VRef.ADS1220_VREF_AVDD_AVSS,
}

ADS1220FIR = ads1220_ns.enum("ADS1220FIR")
FIR = {
    "NONE": ADS1220FIR.ADS1220_NONE,
    "50_60": ADS1220FIR.ADS1220_50HZ_60HZ,
    "50": ADS1220FIR.ADS1220_50HZ,
    "60": ADS1220FIR.ADS1220_60HZ,
}

ADS1220PSW = ads1220_ns.enum("ADS1220PSW")
PSW = {
    "OPEN": ADS1220PSW.ADS1220_ALWAYS_OPEN,
    "SWITCH": ADS1220PSW.ADS1220_SWITCH,
}

ADS1220IdacCurrent = ads1220_ns.enum("ADS1220IdacCurrent")
IDAC_CURRENT = {
    "OFF": ADS1220IdacCurrent.ADS1220_IDAC_OFF,
    "10uA": ADS1220IdacCurrent.ADS1220_IDAC_10_MU_A,
    "50uA": ADS1220IdacCurrent.ADS1220_IDAC_50_MU_A,
    "100uA": ADS1220IdacCurrent.ADS1220_IDAC_100_MU_A,
    "250uA": ADS1220IdacCurrent.ADS1220_IDAC_250_MU_A,
    "500uA": ADS1220IdacCurrent.ADS1220_IDAC_500_MU_A,
    "1000uA": ADS1220IdacCurrent.ADS1220_IDAC_1000_MU_A,
    "1500uA": ADS1220IdacCurrent.ADS1220_IDAC_1500_MU_A,
}

ADS1220IdacRouting = ads1220_ns.enum("ADS1220IdacRouting")
IDAC_ROUTING = {
    "NONE": ADS1220IdacRouting.ADS1220_IDAC_NONE,
    "AIN0_REFP1": ADS1220IdacRouting.ADS1220_IDAC_AIN0_REFP1,
    "AIN1": ADS1220IdacRouting.ADS1220_IDAC_AIN1,
    "AIN2": ADS1220IdacRouting.ADS1220_IDAC_AIN2,
    "AIN3_REFN1": ADS1220IdacRouting.ADS1220_IDAC_AIN3_REFN1,
    "REFP0": ADS1220IdacRouting.ADS1220_IDAC_REFP0,
    "REFN0": ADS1220IdacRouting.ADS1220_IDAC_REFN0,
}

ADS1220DrdyMode = ads1220_ns.enum("ADS1220DrdyMode")
DRDY_MODE = {
    "DRDY_ONLY": ADS1220DrdyMode.ADS1220_DRDY_ONLY,
    "DOUT_DRDY": ADS1220DrdyMode.ADS1220_DOUT_DRDY,
}

ADS1220FaultTestMode = ads1220_ns.enum("ADS1220FaultTestMode")
FAULT_TEST_MODE = {
    "TEST_OFF": ADS1220FaultTestMode.ADS1220_TEST_OFF,
    "TEST_ON": ADS1220FaultTestMode.ADS1220_TEST_ON,
}

def validate_gain(value):
    if isinstance(value, float):
        value = f"{value:0.03f}"
    elif not isinstance(value, str):
        raise cv.Invalid(f'invalid gain "{value}"')

    return cv.enum(GAIN)(value)

ADS1220Sensor = ads1220_ns.class_(
    "ADS1220Sensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

ADS1220GPIOPin = ads1220_ns.class_("ADS1220GPIOPin", cg.GPIOPin)

CONF_ADS1220_ID = "ads1220_id"
CONF_DATARATE = "datarate"
CONF_OP_MODE = "operation_mode"
CONF_CONV_MODE = "conversion_mode"
CONF_VREF = "vref"
CONF_FIR = "fir"
CONF_PSW = "psw"
CONF_IDAC_CURRENT = "idac_current"
CONF_IDAC_1_ROUTING = "idac_1_routing"
CONF_IDAC_2_ROUTING = "idac_2_routing"
CONF_DRDY_MODE = "drdy_mode"
CONF_FAULT_TEST_MODE = "fault_test_mode"
#CONF_DRDY_PIN = "drdy_pin"

TYPE_ADC = "adc"
TYPE_TEMPERATURE = "temperature"

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_ADC: sensor.sensor_schema(
            ADS1220Sensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend(
            {
                cv.GenerateID(CONF_ADS1220_ID): cv.use_id(ADS1220Component),
                cv.Required(CONF_MULTIPLEXER): cv.enum(MUX, upper=True, space="_"),
                cv.Required(CONF_GAIN): validate_gain,
                cv.Optional(CONF_DATARATE, default="DR_LVL_0"): cv.enum(DATARATE, upper=True, space="_"),
                cv.Optional(CONF_OP_MODE, default="TURBO"): cv.enum(OP_MODE, upper=True, space="_"),
                cv.Optional(CONF_CONV_MODE, default="SINGLE_SHOT"): cv.enum(CONV_MODE, upper=True, space="_"),
                cv.Optional(CONF_VREF, default="INTERNAL"): cv.enum(VREF, upper=True, space="_"),
                cv.Optional(CONF_FIR, default="NONE"): cv.enum(FIR, upper=True, space="_"),
                cv.Optional(CONF_PSW, default="OPEN"): cv.enum(PSW, upper=True, space="_"),
                cv.Optional(CONF_IDAC_CURRENT, default="OFF"): cv.enum(IDAC_CURRENT, upper=True, space="_"),
                cv.Optional(CONF_IDAC_1_ROUTING, default="NONE"): cv.enum(IDAC_ROUTING, upper=True, space="_"),
                cv.Optional(CONF_IDAC_2_ROUTING, default="NONE"): cv.enum(IDAC_ROUTING, upper=True, space="_"),
                cv.Optional(CONF_DRDY_MODE, default="DRDY_ONLY"): cv.enum(DRDY_MODE, upper=True, space="_"),
                cv.Optional(CONF_FAULT_TEST_MODE, default="TEST_OFF"): cv.enum(FAULT_TEST_MODE, upper=True, space="_"),
            }
        )
        .extend(cv.polling_component_schema("60s")),
        TYPE_TEMPERATURE: sensor.sensor_schema(
            ADS1220Sensor,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend(
            {
                cv.GenerateID(CONF_ADS1220_ID): cv.use_id(ADS1220Component),
            }
        )
        .extend(cv.polling_component_schema("60s")),
    },
    default_type=TYPE_ADC,
)

async def to_code(config):
    paren = await cg.get_variable(config[CONF_ADS1220_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)

    if config[CONF_TYPE] == TYPE_ADC:
        cg.add(var.set_multiplexer(config[CONF_MULTIPLEXER]))
        cg.add(var.set_gain(config[CONF_GAIN]))
        cg.add(var.set_datarate(config[CONF_DATARATE]))
        cg.add(var.set_operating_mode(config[CONF_OP_MODE]))
        cg.add(var.set_conversion_mode(config[CONF_CONV_MODE]))
        cg.add(var.set_vref_source(config[CONF_VREF]))
        cg.add(var.set_fir_filter(config[CONF_FIR]))
        cg.add(var.set_low_side_power_switch(config[CONF_PSW]))
        cg.add(var.set_idac_current(config[CONF_IDAC_CURRENT]))
        cg.add(var.set_idac_1_routing(config[CONF_IDAC_1_ROUTING]))
        cg.add(var.set_idac_2_routing(config[CONF_IDAC_2_ROUTING]))
        cg.add(var.set_drdy_mode(config[CONF_DRDY_MODE]))
    if config[CONF_TYPE] == TYPE_TEMPERATURE:
        cg.add(var.set_temp_sensor_mode(True))

    cg.add(paren.register_sensor(var))

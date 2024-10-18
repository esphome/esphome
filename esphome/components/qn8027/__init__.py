import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_SENSOR,
    CONF_SOURCE,
    CONF_CURRENT,
    CONF_FREQUENCY,
    CONF_TEXT,
    UNIT_EMPTY,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    ICON_CHIP,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

CODEOWNERS = ["@gabest11"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor", "number", "switch", "select", "text"]
MULTI_CONF = True

UNIT_MEGA_HERTZ = "MHz"
UNIT_KILO_HERTZ = "kHz"
UNIT_MILLI_VOLT = "mV"
UNIT_MICRO_AMPERE = "mA"
UNIT_DECIBEL_MICRO_VOLT = "dBuV"

ICON_VOLUME_MUTE = "mdi:volume-mute"
ICON_EAR_HEARING = "mdi:ear-hearing"
ICON_RADIO_TOWER = "mdi:radio-tower"
ICON_SLEEP = "mdi:sleep"
ICON_SINE_WAVE = "mdi:sine-wave"
ICON_RESISTOR = "mdi:resistor"
ICON_FORMAT_TEXT = "mdi:format-text"

qn8027_ns = cg.esphome_ns.namespace("qn8027")
QN8027Component = qn8027_ns.class_(
    "QN8027Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_QN8027_ID = "qn8027_id"
CONF_RDS = "rds"
CONF_XTAL = "xtal"
# general config
# CONF_FREQUENCY = "frequency"
CONF_DEVIATION = "deviation"
CONF_MUTE = "mute"
CONF_MONO = "mono"
CONF_TX_ENABLE = "tx_enable"
CONF_TX_PILOT = "tx_pilot"
CONF_T1M_SEL = "t1m_sel"
CONF_PRIV_EN = "priv_en"
CONF_PRE_EMPHASIS = "pre_emphasis"
CONF_INPUT_IMPEDANCE = "input_impedance"
CONF_INPUT_GAIN = "input_gain"
CONF_DIGITAL_GAIN = "digital_gain"
CONF_POWER_TARGET = "power_target"
# xtal
# CONF_SOURCE = "source"
# CONF_CURRENT = "current"
# CONF_FREQUENCY = "frequency"
# rds
CONF_ENABLE = "enable"
CONF_DEVIATION = "deviation"
CONF_STATION = "station"
# CONF_TEXT = "text"
# sensor
CONF_AUD_PK = "aud_pk"
CONF_FSM = "fsm"
CONF_CHIP_ID = "chip_id"
CONF_REG30 = "reg30"

SetFrequencyAction = qn8027_ns.class_(
    "SetFrequencyAction", automation.Action, cg.Parented.template(QN8027Component)
)

T1mSel = qn8027_ns.enum("T1mSel", True)
T1M_SEL = {
    "58s": T1mSel.T1M_SEL_58S,
    "59s": T1mSel.T1M_SEL_59S,
    "60s": T1mSel.T1M_SEL_60S,
    "Never": T1mSel.T1M_SEL_NEVER,
}

PreEmphasis = qn8027_ns.enum("PreEmphasis", True)
PRE_EMPHASIS = {
    "50us": PreEmphasis.TC_50US,
    "75us": PreEmphasis.TC_75US,
}

XtalSource = qn8027_ns.enum("XtalSource", True)
XTAL_SOURCE = {
    "CRYSTAL": XtalSource.USE_CRYSTAL_ON_XTAL12,
    "DIGITAL_CLOCK": XtalSource.DIGITAL_CLOCK_ON_XTAL1,
    "SINGLE_END_SIN_WAVE": XtalSource.SINGLE_END_SIN_WAVE_ON_XTAL1,
    "DIFFERENTIAL_SIN_WAVE": XtalSource.DIFFERENTIAL_SIN_WAVE_ON_XTAL12,
}

XtalFrequency = qn8027_ns.enum("XtalFrequency", True)
XTAL_FREQUENCY = {
    "12MHz": XtalFrequency.XSEL_12MHZ,
    "24MHz": XtalFrequency.XSEL_24MHZ,
}

InputImpedance = qn8027_ns.enum("InputImpedance", True)
INPUT_IMPEDANCE = {
    "5kOhm": InputImpedance.VGA_RIN_5KOHM,
    "10kOhm": InputImpedance.VGA_RIN_10KOHM,
    "20kOhm": InputImpedance.VGA_RIN_20KOHM,
    "40kOhm": InputImpedance.VGA_RIN_40KOHM,
}

XTAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SOURCE, default="CRYSTAL"): cv.enum(
            XTAL_SOURCE, upper=True, space="_"
        ),
        cv.Optional(CONF_CURRENT, default=100): cv.float_range(0, 400),
        cv.Optional(CONF_FREQUENCY, default="24MHz"): cv.enum(XTAL_FREQUENCY),
    }
)

RDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default=False): cv.boolean,
        cv.Optional(CONF_DEVIATION, default=2.1): cv.float_range(0, 44.45),
        cv.Optional(CONF_STATION): cv.string,
        cv.Optional(CONF_TEXT): cv.string,
    }
)

SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_AUD_PK): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLI_VOLT,
            # accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_FSM): text_sensor.text_sensor_schema(
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_CHIP,
        ),
        cv.Optional(CONF_CHIP_ID): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_CHIP,
        ),
        cv.Optional(CONF_REG30): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_CHIP,
        ),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(QN8027Component),
            cv.Optional(CONF_FREQUENCY, default=87.50): cv.float_range(76, 108),
            cv.Optional(CONF_DEVIATION, default=74.82): cv.float_range(0, 147.9),
            cv.Optional(CONF_MUTE, default=False): cv.boolean,
            cv.Optional(CONF_MONO, default=False): cv.boolean,
            cv.Optional(CONF_TX_ENABLE, default=True): cv.boolean,
            cv.Optional(CONF_TX_PILOT, default=9): cv.All(
                cv.uint8_t, cv.Range(min=7, max=10)
            ),
            cv.Optional(CONF_T1M_SEL, default="60s"): cv.enum(T1M_SEL),
            cv.Optional(CONF_PRIV_EN, default=False): cv.boolean,
            cv.Optional(CONF_PRE_EMPHASIS, default="75us"): cv.enum(PRE_EMPHASIS),
            cv.Optional(CONF_INPUT_IMPEDANCE, default="20kOhm"): cv.enum(
                INPUT_IMPEDANCE
            ),
            cv.Optional(CONF_INPUT_GAIN, default=3): cv.All(
                cv.uint8_t, cv.Range(min=0, max=5)
            ),
            cv.Optional(CONF_DIGITAL_GAIN, default=0): cv.All(
                cv.uint8_t, cv.Range(min=0, max=2)
            ),
            cv.Optional(CONF_POWER_TARGET, default=117.5): cv.float_range(83.4, 117.5),
            cv.Optional(CONF_XTAL): XTAL_SCHEMA,
            cv.Optional(CONF_RDS): RDS_SCHEMA,
            cv.Optional(CONF_SENSOR): SENSOR_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x2C))
)


FREQUENCY_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(QN8027Component),
        cv.Required(CONF_FREQUENCY): cv.float_range(min=76, max=108),
    }
)


@automation.register_action(
    "qn8027.set_frequency", SetFrequencyAction, FREQUENCY_SCHEMA
)
async def tune_frequency_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if frequency := config.get(CONF_FREQUENCY):
        template_ = await cg.templatable(frequency, args, cg.float_)
        cg.add(var.set_frequency(template_))
    return var


async def set_var(config, id, setter):
    if c := config.get(id):
        cg.add(setter(c))


async def set_sensor(config, id, setter):
    if c := config.get(id):
        s = await sensor.new_sensor(c)
        cg.add(setter(s))


async def set_text_sensor(config, id, setter):
    if c := config.get(id):
        s = await text_sensor.new_text_sensor(c)
        cg.add(setter(s))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await set_var(config, CONF_FREQUENCY, var.set_frequency)
    await set_var(config, CONF_DEVIATION, var.set_deviation)
    await set_var(config, CONF_MUTE, var.set_mute)
    await set_var(config, CONF_MONO, var.set_mono)
    await set_var(config, CONF_TX_ENABLE, var.set_tx_enable)
    await set_var(config, CONF_T1M_SEL, var.set_t1m_sel)
    await set_var(config, CONF_PRE_EMPHASIS, var.set_pre_emphasis)
    await set_var(config, CONF_PRIV_EN, var.set_priv_en)
    await set_var(config, CONF_INPUT_IMPEDANCE, var.set_input_impedance)
    await set_var(config, CONF_INPUT_GAIN, var.set_input_gain)
    await set_var(config, CONF_DIGITAL_GAIN, var.set_digital_gain)
    await set_var(config, CONF_POWER_TARGET, var.set_power_target)
    if xtal_config := config.get(CONF_XTAL):
        await set_var(xtal_config, CONF_SOURCE, var.set_xtal_source)
        await set_var(xtal_config, CONF_CURRENT, var.set_xtal_current)
        await set_var(xtal_config, CONF_FREQUENCY, var.set_xtal_frequency)
    if rds_config := config.get(CONF_RDS):
        await set_var(rds_config, CONF_ENABLE, var.set_rds_enable)
        await set_var(rds_config, CONF_STATION, var.set_rds_station)
        await set_var(rds_config, CONF_TEXT, var.set_rds_text)
        await set_var(rds_config, CONF_DEVIATION, var.set_rds_deviation)
    if sensor_config := config.get(CONF_SENSOR):
        await set_sensor(sensor_config, CONF_AUD_PK, var.set_aud_pk_sensor)
        await set_text_sensor(sensor_config, CONF_FSM, var.set_fsm_text_sensor)
        await set_text_sensor(sensor_config, CONF_CHIP_ID, var.set_chip_id_text_sensor)
        await set_sensor(sensor_config, CONF_REG30, var.set_reg30_sensor)

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import i2c, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    UNIT_VOLT,
    UNIT_EMPTY,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    ICON_CHIP,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

CODEOWNERS = ["@gabest11"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor", "number", "switch", "select"]
MULTI_CONF = True

UNIT_MEGA_HERTZ = "MHz"
UNIT_KILO_HERTZ = "KHz"
UNIT_MILLI_VOLT = "mV"
UNIT_MICRO_AMPERE = "mA"
UNIT_DECIBEL_MICRO_VOLT = "dBµV"

ICON_VOLUME_MUTE = "mdi:volume-mute"
ICON_EAR_HEARING = "mdi:ear-hearing"
ICON_RADIO_TOWER = "mdi:radio-tower"
ICON_SLEEP = "mdi:sleep"
ICON_SINE_WAVE = "mdi:sine-wave"
ICON_RESISTOR = "mdi:resistor"
ICON_FORMAT_TEXT = "mdi:format-text"

qn8027_ns = cg.esphome_ns.namespace("qn8027")
QN8027Component = qn8027_ns.class_("QN8027Component", cg.PollingComponent, i2c.I2CDevice)

CONF_QN8027_ID = "qn8027_id"
#CONF_FREQUENCY = "frequency"
CONF_FREQUENCY_DEVIATION = "frequency_deviation"
CONF_MUTE = "mute"
CONF_MONO = "mono"
CONF_TX_ENABLE = "tx_enable"
CONF_TX_PILOT = "tx_pilot"
CONF_T1M_SEL = "t1m_sel"
CONF_PRIV_EN = "priv_en"
CONF_PRE_EMPHASIS = "pre_emphasis"
CONF_XTAL_SOURCE = "xtal_source"
CONF_XTAL_CURRENT = "xtal_current"
CONF_XTAL_FREQUENCY = "xtal_frequency"
CONF_INPUT_IMPEDANCE = "input_impedance"
CONF_INPUT_GAIN = "input_gain"
CONF_DIGITAL_GAIN = "digital_gain"
CONF_POWER_TARGET = "power_target"
CONF_RDS_ENABLE = "rds_enable"
CONF_RDS_FREQUENCY_DEVIATION = "rds_frequency_deviation"
CONF_RDS_STATION = "rds_station"
CONF_RDS_TEXT = "rds_text"
CONF_AUD_PK = "aud_pk"
CONF_FSM = "fsm"
CONF_CHIP_ID = "chip_id"

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

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(QN8027Component),
            cv.Optional(CONF_FREQUENCY, default=87.50): cv.float_range(76, 108),
            cv.Optional(CONF_FREQUENCY_DEVIATION, default=74.82): cv.float_range(0, 147.9),
            cv.Optional(CONF_MUTE, default=False): cv.boolean,
            cv.Optional(CONF_MONO, default=False): cv.boolean,
            cv.Optional(CONF_TX_ENABLE, default=True): cv.boolean,
            cv.Optional(CONF_TX_PILOT, default=9): cv.All(cv.uint8_t, cv.Range(min=7, max=10)),
            cv.Optional(CONF_T1M_SEL, default="60s"): cv.enum(T1M_SEL),
            cv.Optional(CONF_PRIV_EN, default=False): cv.boolean,
            cv.Optional(CONF_PRE_EMPHASIS, default="75us"): cv.enum(PRE_EMPHASIS),
            cv.Optional(CONF_XTAL_SOURCE, default="CRYSTAL"): cv.enum(XTAL_SOURCE, upper=True, space="_"),
            cv.Optional(CONF_XTAL_CURRENT, default=100): cv.float_range(0, 400),
            cv.Optional(CONF_XTAL_FREQUENCY, default="24MHz"): cv.enum(XTAL_FREQUENCY),
            cv.Optional(CONF_INPUT_IMPEDANCE, default="20kOhm"): cv.enum(INPUT_IMPEDANCE),
            cv.Optional(CONF_INPUT_GAIN, default=3): cv.All(cv.uint8_t, cv.Range(min=0, max=5)),
            cv.Optional(CONF_DIGITAL_GAIN, default=0): cv.All(cv.uint8_t, cv.Range(min=0, max=2)),
            cv.Optional(CONF_POWER_TARGET, default=117.5): cv.float_range(83.4, 117.5),
            cv.Optional(CONF_RDS_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_RDS_FREQUENCY_DEVIATION, default=2.1): cv.float_range(0, 44.45),
            cv.Optional(CONF_RDS_STATION): cv.string,
            cv.Optional(CONF_RDS_TEXT): cv.string,
            cv.Optional(CONF_AUD_PK): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLI_VOLT,
                #accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_FSM): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon=ICON_CHIP,
            ),
            cv.Optional(CONF_CHIP_ID): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon=ICON_CHIP,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x2C))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if conf_frequency := config.get(CONF_FREQUENCY):
        cg.add(var.set_frequency(conf_frequency))
    if conf_frequency_deviation := config.get(CONF_FREQUENCY_DEVIATION):
        cg.add(var.set_frequency_deviation(conf_frequency_deviation))
    if conf_mute := config.get(CONF_MUTE):
        cg.add(var.set_mute(conf_mute))
    if conf_mono := config.get(CONF_MONO):
        cg.add(var.set_mono(conf_mono))
    if conf_tx_enable := config.get(CONF_TX_ENABLE):
        cg.add(var.set_tx_enable(conf_tx_enable))
    if conf_t1m_sel := config.get(CONF_T1M_SEL):
        cg.add(var.set_t1m_sel(conf_t1m_sel))
    if conf_pre_emphasis := config.get(CONF_PRE_EMPHASIS):
        cg.add(var.set_pre_emphasis(conf_pre_emphasis))
    if conf_priv_en := config.get(CONF_PRIV_EN):
        cg.add(var.set_priv_en(conf_priv_en))
    if conf_xtal_source := config.get(CONF_XTAL_SOURCE):
        cg.add(var.set_xtal_source(conf_xtal_source))
    if conf_xtal_current := config.get(CONF_XTAL_CURRENT):
        cg.add(var.set_xtal_current(conf_xtal_current))
    if conf_xtal_frequency := config.get(CONF_XTAL_FREQUENCY):
        cg.add(var.set_xtal_frequency(conf_xtal_frequency))
    if conf_input_impedance := config.get(CONF_INPUT_IMPEDANCE):
        cg.add(var.set_input_impedance(conf_input_impedance))
    if conf_input_gain := config.get(CONF_INPUT_GAIN):
        cg.add(var.set_input_gain(conf_input_gain))
    if conf_digital_gain := config.get(CONF_DIGITAL_GAIN):
        cg.add(var.set_digital_gain(conf_digital_gain))
    if conf_power_target := config.get(CONF_POWER_TARGET):
        cg.add(var.set_power_target(conf_power_target))
    if conf_rds_enable := config.get(CONF_RDS_ENABLE):
        cg.add(var.set_rds_enable(conf_rds_enable))
    if conf_rds_station := config.get(CONF_RDS_STATION):
        cg.add(var.set_rds_station(conf_rds_station))
    if conf_rds_text := config.get(CONF_RDS_TEXT):
        cg.add(var.set_rds_text(conf_rds_text))
    if conf_rds_frequency_deviation := config.get(CONF_RDS_FREQUENCY_DEVIATION):
        cg.add(var.set_rds_frequency_deviation(conf_rds_frequency_deviation))
    if conf_aud_pk := config.get(CONF_AUD_PK):
        s = await sensor.new_sensor(conf_aud_pk)
        cg.add(var.set_aud_pk_sensor(s))
    if conf_fsm := config.get(CONF_FSM):
        s = await text_sensor.new_text_sensor(conf_fsm)
        cg.add(var.set_fsm_text_sensor(s))
    if conf_chip_id := config.get(CONF_CHIP_ID):
        s = await text_sensor.new_text_sensor(conf_chip_id)
        cg.add(var.set_chip_id_text_sensor(s))


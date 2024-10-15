import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import i2c, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
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
UNIT_DECIBEL_MICRO_VOLT = "dBµV"

ICON_VOLUME_MUTE = "mdi:volume-mute"
ICON_EAR_HEARING = "mdi:ear-hearing"
ICON_RADIO_TOWER = "mdi:radio-tower"
ICON_SLEEP = "mdi:sleep"
ICON_SINE_WAVE = "mdi:sine-wave"
ICON_RESISTOR = "mdi:resistor"
ICON_FORMAT_TEXT = "mdi:format-text"

si4713_ns = cg.esphome_ns.namespace("si4713")
Si4713Component = si4713_ns.class_(
    "Si4713Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_SI4713_ID = "si4713_id"
CONF_RESET_PIN = "reset_pin"
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
CONF_REG30 = "reg30"

SetFrequencyAction = si4713_ns.class_(
    "SetFrequencyAction", automation.Action, cg.Parented.template(Si4713Component)
)

#T1mSel = si4713_ns.enum("T1mSel", True)
#T1M_SEL = {
#    "58s": T1mSel.T1M_SEL_58S,
#    "59s": T1mSel.T1M_SEL_59S,
#    "60s": T1mSel.T1M_SEL_60S,
#    "Never": T1mSel.T1M_SEL_NEVER,
#}


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Si4713Component),
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_FREQUENCY, default=87.50): cv.float_range(76, 108),
            cv.Optional(CONF_MUTE, default=False): cv.boolean,
            cv.Optional(CONF_MONO, default=False): cv.boolean,
#            cv.Optional(CONF_T1M_SEL, default="60s"): cv.enum(T1M_SEL),
            cv.Optional(CONF_RDS_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_RDS_STATION): cv.string,
            cv.Optional(CONF_RDS_TEXT): cv.string,
            cv.Optional(CONF_CHIP_ID): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                icon=ICON_CHIP,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x63))
)


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
    reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))
    await set_var(config, CONF_FREQUENCY, var.set_frequency)
    await set_var(config, CONF_MUTE, var.set_mute)
    await set_var(config, CONF_MONO, var.set_mono)
#    await set_var(config, CONF_T1M_SEL, var.set_t1m_sel)
    await set_var(config, CONF_RDS_ENABLE, var.set_rds_enable)
    await set_var(config, CONF_RDS_STATION, var.set_rds_station)
    await set_var(config, CONF_RDS_TEXT, var.set_rds_text)
    await set_text_sensor(config, CONF_CHIP_ID, var.set_chip_id_text_sensor)

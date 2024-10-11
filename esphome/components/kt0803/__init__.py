import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, sensor, text_sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_EMPTY,
)

CODEOWNERS = ["@gabest11"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor", "number", "switch", "select", "text"]
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

kt0803_ns = cg.esphome_ns.namespace("kt0803")
KT0803Component = kt0803_ns.class_(
    "KT0803Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_KT0803_ID = "kt0803_id"
CONF_CHIP_ID = "chip_id"
CONF_PW_OK = "pw_ok"
CONF_SLNCID = "slncid"

SetFrequencyAction = kt0803_ns.class_(
    "SetFrequencyAction", automation.Action, cg.Parented.template(KT0803Component)
)

ChipId = kt0803_ns.enum("ChipId", True)
CHIP_ID = {
    "KT0803": ChipId.KT0803,
    "KT0803K": ChipId.KT0803K,
    "KT0803M": ChipId.KT0803M,
    "KT0803L": ChipId.KT0803L,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(KT0803Component),
            cv.Required(CONF_CHIP_ID): cv.enum(CHIP_ID),
            cv.Optional(CONF_FREQUENCY, default=87.50): cv.float_range(70, 108),
            cv.Optional(CONF_PW_OK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_POWER,
                icon=ICON_RADIO_TOWER,
            ),
            cv.Optional(CONF_SLNCID): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
                icon=ICON_VOLUME_MUTE,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x3E))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_chip_id(config.get(CONF_CHIP_ID)))
    if conf_frequency := config.get(CONF_FREQUENCY):
        cg.add(var.set_frequency(conf_frequency))
    if conf_pw_ok := config.get(CONF_PW_OK):
        s = await binary_sensor.new_binary_sensor(conf_pw_ok)
        cg.add(var.set_pw_ok_binary_sensor(s))
    if conf_slncid := config.get(CONF_SLNCID):
        s = await binary_sensor.new_binary_sensor(conf_slncid)
        cg.add(var.set_slncid_binary_sensor(s))

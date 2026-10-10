import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_CHANNELS,
    CONF_FREQUENCY,
    CONF_I2C_ID,
    CONF_ID,
)
from esphome.types import ConfigType

CODEOWNERS = ["@andreashergert1984"]
DOMAIN = "tca9548a"

DEPENDENCIES = ["i2c"]

tca9548a_ns = cg.esphome_ns.namespace("tca9548a")
TCA9548AComponent = tca9548a_ns.class_("TCA9548AComponent", cg.Component, i2c.I2CDevice)
TCA9548AChannel = tca9548a_ns.class_("TCA9548AChannel", i2c.I2CBus)

MULTI_CONF = True

CONF_BUS_ID = "bus_id"
# Sizes the upstream bus's frequency table; a port frequency is only compiled
# in when one is configured
_request_port_frequency_slot = cg.slot_counter("I2C_PORT_FREQUENCY_COUNT")

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TCA9548AComponent),
            cv.Optional(CONF_CHANNELS, default=[]): cv.ensure_list(
                {
                    cv.Required(CONF_BUS_ID): cv.declare_id(TCA9548AChannel),
                    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
                    cv.Optional(CONF_FREQUENCY): cv.All(
                        cv.frequency, cv.Range(min=0, min_included=False)
                    ),
                }
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x70))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for conf in config[CONF_CHANNELS]:
        chan = cg.new_Pvariable(conf[CONF_BUS_ID])
        cg.add(chan.set_parent(var))
        cg.add(chan.set_channel(conf[CONF_CHANNEL]))
        if (frequency := conf.get(CONF_FREQUENCY)) is not None:
            _request_port_frequency_slot(str(config[CONF_I2C_ID]))
            cg.add(chan.set_frequency(int(frequency)))

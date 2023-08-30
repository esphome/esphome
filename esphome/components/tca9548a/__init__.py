import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_CHANNEL, CONF_CHANNELS, CONF_ID, CONF_SCAN

CODEOWNERS = ["@andreashergert1984"]

DEPENDENCIES = ["i2c"]

tca9548a_ns = cg.esphome_ns.namespace("tca9548a")
TCA9548AComponent = tca9548a_ns.class_("TCA9548AComponent", cg.Component, i2c.I2CDevice)
TCA9548AChannel = tca9548a_ns.class_("TCA9548AChannel", i2c.I2CBus)

CONF_BUS_ID = "bus_id"

TCA9548A_COMPONENT_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TCA9548AComponent),
            cv.Optional(CONF_SCAN): cv.invalid("This option has been removed"),
            cv.Optional(CONF_CHANNELS, default=[]): cv.ensure_list(
                {
                    cv.Required(CONF_BUS_ID): cv.declare_id(TCA9548AChannel),
                    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
                }
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x70))
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.ensure_list(TCA9548A_COMPONENT_SCHEMA)


async def to_code(config):
    for component in config:
        var = cg.new_Pvariable(component[CONF_ID])
        await cg.register_component(var, component)
        await i2c.register_i2c_device(var, component)

        # If more than one tca9548a is configured, disable all channels after all IO to avoid address conflicts
        cg.add(var.set_disable_channels_after_io(len(config) > 1))
        for conf in component[CONF_CHANNELS]:
            chan = cg.new_Pvariable(conf[CONF_BUS_ID])
            cg.add(chan.set_parent(var))
            cg.add(chan.set_channel(conf[CONF_CHANNEL]))

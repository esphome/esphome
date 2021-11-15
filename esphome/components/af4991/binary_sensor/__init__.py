import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_PIN
from .. import CONF_AF4991_ID, af4991_ns, AF4991

DEPENDENCIES = ["af4991"]

AF4991BinarySensor = af4991_ns.class_(
    "AF4991BinarySensor", cg.Component, binary_sensor.BinarySensor
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AF4991BinarySensor),
            cv.Required(CONF_AF4991_ID): cv.use_id(AF4991),
            cv.Optional(
                CONF_PIN
            ): cv.int_,  # The pin that is being used on the connected I2C Board
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    parent = await cg.get_variable(config[CONF_AF4991_ID])
    cg.add(var.set_parent(parent))

    if CONF_PIN in config:
        cg.add(var.set_pin(config[CONF_PIN]))

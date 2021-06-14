import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from .. import SX1509Component, sx1509_ns, CONF_SX1509_ID

CONF_ROW = "row"
CONF_COL = "col"

DEPENDENCIES = ["sx1509"]

SX1509BinarySensor = sx1509_ns.class_("SX1509BinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SX1509BinarySensor),
        cv.GenerateID(CONF_SX1509_ID): cv.use_id(SX1509Component),
        cv.Required(CONF_ROW): cv.int_range(min=0, max=4),
        cv.Required(CONF_COL): cv.int_range(min=0, max=4),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)
    hub = await cg.get_variable(config[CONF_SX1509_ID])
    cg.add(var.set_row_col(config[CONF_ROW], config[CONF_COL]))

    cg.add(hub.register_keypad_binary_sensor(var))

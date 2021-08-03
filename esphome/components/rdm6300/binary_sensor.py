import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, rdm6300
from esphome.const import CONF_UID, CONF_ID
from . import rdm6300_ns

DEPENDENCIES = ["rdm6300"]

CONF_RDM6300_ID = "rdm6300_id"
RDM6300BinarySensor = rdm6300_ns.class_(
    "RDM6300BinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RDM6300BinarySensor),
        cv.GenerateID(CONF_RDM6300_ID): cv.use_id(rdm6300.RDM6300Component),
        cv.Required(CONF_UID): cv.uint32_t,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)

    hub = await cg.get_variable(config[CONF_RDM6300_ID])
    cg.add(hub.register_card(var))
    cg.add(var.set_id(config[CONF_UID]))

import esphome.codegen as cg
from esphome.components import binary_sensor, rdm6300
import esphome.config_validation as cv
from esphome.const import CONF_UID

from . import rdm6300_ns

DEPENDENCIES = ["rdm6300"]

CONF_RDM6300_ID = "rdm6300_id"
RDM6300BinarySensor = rdm6300_ns.class_(
    "RDM6300BinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(RDM6300BinarySensor).extend(
    {
        cv.GenerateID(CONF_RDM6300_ID): cv.use_id(rdm6300.RDM6300Component),
        cv.Required(CONF_UID): cv.uint32_t,
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)

    hub = await cg.get_variable(config[CONF_RDM6300_ID])
    cg.add(hub.register_card(var))
    cg.add(var.set_id(config[CONF_UID]))

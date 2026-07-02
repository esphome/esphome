import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY

from .. import CONF_HOERMANN_ID, Hoermann, hoermann_ns

DEPENDENCIES = ["hoermann"]

HoermannConnectedBinarySensor = hoermann_ns.class_(
    "HoermannConnectedBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONF_IS_CONNECTED = "is_connected"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HOERMANN_ID): cv.use_id(Hoermann),
        cv.Optional(CONF_IS_CONNECTED): binary_sensor.binary_sensor_schema(
            HoermannConnectedBinarySensor, device_class=DEVICE_CLASS_CONNECTIVITY
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HOERMANN_ID])
    if conf := config.get(CONF_IS_CONNECTED):
        var = await binary_sensor.new_binary_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))

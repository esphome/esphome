import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, DEVICE_CLASS_POWER

from .. import CONF_PARENT_ID, FendtCaravanHubBase

CONF_POWER_STATUS = "power_status"

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_POWER_STATUS: binary_sensor.binary_sensor_schema(
            icon="mdi:power-plug",
            device_class=DEVICE_CLASS_POWER,
        ).extend(
            {
                cv.Required(CONF_PARENT_ID): cv.use_id(FendtCaravanHubBase),
            }
        )
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_binary_sensor")(var))

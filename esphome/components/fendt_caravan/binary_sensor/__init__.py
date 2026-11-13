from esphome import final_validate as fv
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, DEVICE_CLASS_PLUG as DEVICE_CLASS_PLUG
from esphome.types import ConfigType

from .. import CONF_PARENT_ID, DOMAIN, MainControlUnitHub as MainControlUnitHub

CONF_POWER_STATUS = "power_status"

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_POWER_STATUS: binary_sensor.binary_sensor_schema(
            icon="mdi:power-plug",
        ),
    }
)


# The hub keeps one pointer per type, so a second entry would take over the first one's updates
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="binary_sensor",
    platform=DOMAIN,
    keys=(CONF_PARENT_ID, CONF_TYPE),
    message="Only one fendt_caravan binary sensor of type '{type}' is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    setter = {
        CONF_POWER_STATUS: hub.set_power_status_binary_sensor,
    }[config[CONF_TYPE]]
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(setter(var))

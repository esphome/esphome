import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_CURRENT, CONF_VOLTAGE, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_FUSB302B_ID, FUSB302B

DEPENDENCIES = ["fusb302b"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FUSB302B_ID): cv.use_id(FUSB302B),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            kind=sensor.VOLTAGE,
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            kind=sensor.CURRENT,
            accuracy_decimals=2,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_FUSB302B_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, hub.set_voltage_sensor)
    await sensors(CONF_CURRENT, hub.set_current_sensor)

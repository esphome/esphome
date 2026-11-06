import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_VALUE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_INDEX
from esphome.types import ConfigType

from .. import (
    CONF_CANOPEN_ID,
    CONF_NODE_ID,
    CONF_SUBINDEX,
    SDO_SCHEMA,
    CANopenComponent,
    canopen_ns,
)

DEPENDENCIES = ["canopen"]

CANopenSensor = canopen_ns.class_("CANopenSensor", sensor.Sensor, cg.PollingComponent)
ValueType = canopen_ns.enum("ValueType", is_class=True)
VALUE_TYPES = ValueType.options(
    "UINT8",
    "INT8",
    "UINT16",
    "INT16",
    "UINT32",
    "INT32",
    "FLOAT32",
    prefix="VALUE_TYPE_",
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(CANopenSensor)
    .extend(SDO_SCHEMA)
    .extend(
        {
            cv.GenerateID(CONF_CANOPEN_ID): cv.use_id(CANopenComponent),
            cv.Required(CONF_VALUE_TYPE): cv.enum(VALUE_TYPES, upper=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_CANOPEN_ID])
    var = await sensor.new_sensor(
        config,
        hub,
        config[CONF_NODE_ID],
        config[CONF_INDEX],
        config[CONF_SUBINDEX],
        config[CONF_VALUE_TYPE],
    )
    await cg.register_component(var, config)

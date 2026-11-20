import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from ..touchscreen import CST226Touchscreen

CONF_CST226_ID = "cst226_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_CST226_ID): cv.use_id(CST226Touchscreen),
    }
)

# Each touchscreen keeps its buttons in a fixed size list; the define is the largest count any one needs
_request_button_slot = cg.slot_counter("CST226_BUTTON_COUNT")


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_CST226_ID])
    var = await binary_sensor.new_binary_sensor(config)
    _request_button_slot(str(hub))
    cg.add(hub.add_button_binary_sensor(var))

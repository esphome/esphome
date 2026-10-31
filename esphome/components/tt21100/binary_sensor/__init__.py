import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_INDEX
from esphome.types import ConfigType

from ..touchscreen import TT21100Touchscreen

CONF_TT21100_ID = "tt21100_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_TT21100_ID): cv.use_id(TT21100Touchscreen),
        cv.Required(CONF_INDEX): cv.int_range(min=0, max=3),
    }
)

# Each touchscreen keeps its buttons in a fixed size list; the define is the largest count any one needs
_request_button_slot = cg.slot_counter("TT21100_BUTTON_COUNT")


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TT21100_ID])
    var = await binary_sensor.new_binary_sensor(config)
    _request_button_slot(str(hub))
    cg.add(hub.add_button_binary_sensor(var, config[CONF_INDEX]))

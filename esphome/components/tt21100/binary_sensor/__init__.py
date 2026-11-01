from dataclasses import dataclass, field
from operator import itemgetter

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_INDEX
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

from .. import DOMAIN
from ..touchscreen import TT21100Touchscreen

CONF_TT21100_ID = "tt21100_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_TT21100_ID): cv.use_id(TT21100Touchscreen),
        cv.Required(CONF_INDEX): cv.int_range(min=0, max=3),
    }
)


_request_button_slot = cg.slot_counter("TT21100_BUTTON_COUNT")


@dataclass
class _ButtonData:
    # (index, hub, binary sensor) in config order
    buttons: list[tuple[int, cg.MockObj, cg.MockObj]] = field(default_factory=list)


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_buttons() -> None:
    # Stable sort: each hub's keys publish in index order, config order within one
    for index, hub, var in sorted(CORE.data[DOMAIN].buttons, key=itemgetter(0)):
        cg.add(hub.add_button_binary_sensor(var, index))


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TT21100_ID])
    var = await binary_sensor.new_binary_sensor(config)
    _request_button_slot(str(hub))
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _ButtonData()
        CORE.add_job(_add_buttons)
    CORE.data[DOMAIN].buttons.append((config[CONF_INDEX], hub, var))

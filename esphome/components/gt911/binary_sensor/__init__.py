from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_INDEX
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

from .. import DOMAIN
from ..touchscreen import GT911Touchscreen

CONF_GT911_ID = "gt911_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_GT911_ID): cv.use_id(GT911Touchscreen),
        cv.Optional(CONF_INDEX, default=0): cv.int_range(min=0, max=3),
    }
)


@dataclass
class _ButtonData:
    # Each hub's binary sensors and their key index, by hub id
    hubs: dict[str, tuple[cg.MockObj, list[tuple[int, cg.MockObj]]]] = field(
        default_factory=dict
    )


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_buttons() -> None:
    hubs = CORE.data[DOMAIN].hubs
    for hub, buttons in hubs.values():
        # A report publishes in list order: by index, and stable so config order within one
        for index, var in sorted(buttons, key=lambda button: button[0]):
            cg.add(hub.add_button_binary_sensor(var, index))
    # Each hub keeps its buttons in a fixed size list sized for the largest one
    cg.add_define(
        "GT911_BUTTON_COUNT", max(len(buttons) for _, buttons in hubs.values())
    )


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_GT911_ID])
    var = await binary_sensor.new_binary_sensor(config)
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _ButtonData()
        CORE.add_job(_add_buttons)
    hubs = CORE.data[DOMAIN].hubs
    hubs.setdefault(str(hub), (hub, []))[1].append((config[CONF_INDEX], var))

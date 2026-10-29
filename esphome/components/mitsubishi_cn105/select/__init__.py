import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    CONF_MITSUBISHI_CN105_ID,
    MITSUBISHI_CN105_DEVICE_SCHEMA,
    VERTICAL_VANE_DIRECTIONS,
)

DEPENDENCIES = ["mitsubishi_cn105"]

CONF_VERTICAL_VANE_DIRECTION = "vertical_vane_direction"
CONF_HORIZONTAL_VANE_DIRECTION = "horizontal_vane_direction"

# Positions are numbered from the far left (1) to the far right (5).
# The order must match MitsubishiCN105::WideVaneMode.
HORIZONTAL_VANE_DIRECTIONS = ["1", "2", "3", "4", "5", "Split", "Swing"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VERTICAL_VANE_DIRECTION): select.select_schema(
            select.ApplySelect,
            icon="mdi:arrow-up-down",
        ),
        cv.Optional(CONF_HORIZONTAL_VANE_DIRECTION): select.select_schema(
            select.ApplySelect,
            icon="mdi:arrow-left-right",
        ),
    }
).extend(MITSUBISHI_CN105_DEVICE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MITSUBISHI_CN105_ID])
    selects = select.sub_selects(config, parent=hub)
    # The options are in the order of the vane mode enums; the hub publishes the state
    await selects(
        CONF_VERTICAL_VANE_DIRECTION,
        hub.set_vertical_vane_direction_select,
        options=[direction.capitalize() for direction in VERTICAL_VANE_DIRECTIONS],
        apply="control_vertical_vane_direction(index)",
    )
    await selects(
        CONF_HORIZONTAL_VANE_DIRECTION,
        hub.set_horizontal_vane_direction_select,
        options=HORIZONTAL_VANE_DIRECTIONS,
        apply="control_horizontal_vane_direction(index)",
    )

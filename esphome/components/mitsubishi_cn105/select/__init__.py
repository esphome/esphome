import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_MITSUBISHI_CN105_ID,
    DOMAIN,
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

# The hub keeps one pointer per direction, so a second entry would take over the first one's updates
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="select",
    platform=DOMAIN,
    keys=(CONF_MITSUBISHI_CN105_ID,),
    message="Only one mitsubishi_cn105 select entry is allowed per hub; put both vane directions in it",
)


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

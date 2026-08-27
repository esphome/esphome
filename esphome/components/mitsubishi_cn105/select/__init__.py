import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import (
    MITSUBISHI_CN105_DEVICE_SCHEMA,
    VERTICAL_VANE_DIRECTIONS,
    MitsubishiCN105Component,
    mitsubishi_ns,
    register_mitsubishi_cn105_device,
)

DEPENDENCIES = ["mitsubishi_cn105"]

CONF_VERTICAL_VANE_DIRECTION = "vertical_vane_direction"

MitsubishiCN105VerticalVaneDirectionSelect = mitsubishi_ns.class_(
    "MitsubishiCN105VerticalVaneDirectionSelect",
    select.Select,
    cg.Component,
    cg.Parented.template(MitsubishiCN105Component),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VERTICAL_VANE_DIRECTION): select.select_schema(
            MitsubishiCN105VerticalVaneDirectionSelect,
            icon="mdi:arrow-up-down",
        ),
    }
).extend(MITSUBISHI_CN105_DEVICE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    if vertical_vane_direction := config.get(CONF_VERTICAL_VANE_DIRECTION):
        var = cg.new_Pvariable(vertical_vane_direction[CONF_ID])
        await cg.register_component(var, vertical_vane_direction)
        await select.register_select(
            var,
            vertical_vane_direction,
            options=[direction.capitalize() for direction in VERTICAL_VANE_DIRECTIONS],
        )
        await register_mitsubishi_cn105_device(var, config)

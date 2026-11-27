import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_MODE
from esphome.types import ConfigType

from .. import (
    CONF_DUCO_ID,
    DUCO_COMPONENT_SCHEMA,
    DucoDevice,
    duco_ns,
    register_duco_device,
)

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx", "@w1ll14m"]

# Same order as MODE_CODES in select.cpp
DUCO_MODE_OPTIONS = [
    "AUTO",
    "MAN1",
    "MAN2",
    "MAN3",
    "EMPT",
    "CNT1",
    "CNT2",
    "CNT3",
    "MAN1x2",
    "MAN2x2",
    "MAN3x2",
    "MAN1x3",
    "MAN2x3",
    "MAN3x3",
]

DucoSelect = duco_ns.class_("DucoSelect", DucoDevice, select.Select)

MODE_SCHEMA = (
    select.select_schema(DucoSelect)
    .extend({cv.Optional(CONF_ADDRESS, default=1): cv.int_range(1, 68)})
    .extend(cv.polling_component_schema("3s"))
)
CONFIG_SCHEMA = cv.Any(
    MODE_SCHEMA.extend(DUCO_COMPONENT_SCHEMA),
    DUCO_COMPONENT_SCHEMA.extend({cv.Required(CONF_MODE): cv.ensure_list(MODE_SCHEMA)}),
)


async def to_code(config: ConfigType) -> None:
    for entity_config in config.get(CONF_MODE, [config]):
        var = await select.new_select(
            entity_config, options=DUCO_MODE_OPTIONS, fixed_options=True
        )
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))
        await register_duco_device(var, entity_config, config[CONF_DUCO_ID])

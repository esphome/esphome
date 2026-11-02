import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, Duco, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoReboot = duco_ns.class_("DucoReboot", button.Button, cg.Parented.template(Duco))

TYPE_REBOOT = "reboot"
CONF_REBOOT = "reboot"

REBOOT_SCHEMA = button.button_schema(DucoReboot).extend(
    {
        cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=0x45),
    }
)

LEGACY_SCHEMA = REBOOT_SCHEMA.extend(DUCO_COMPONENT_SCHEMA).extend(
    {
        cv.Optional(CONF_TYPE): cv.one_of(TYPE_REBOOT),
    }
)

CONFIG_SCHEMA = cv.Any(
    LEGACY_SCHEMA,
    DUCO_COMPONENT_SCHEMA.extend(
        {
            cv.Optional(CONF_REBOOT): cv.ensure_list(REBOOT_SCHEMA),
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    if CONF_REBOOT not in config:
        var = await button.new_button(config)
        await cg.register_parented(var, hub)
        cg.add(var.set_address(config[CONF_ADDRESS]))
        return

    for entity_config in config.get(CONF_REBOOT, []):
        var = await button.new_button(entity_config)
        await cg.register_parented(var, hub)
        cg.add(var.set_address(entity_config[CONF_ADDRESS]))

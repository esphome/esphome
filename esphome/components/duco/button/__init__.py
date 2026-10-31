import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, Duco, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoReboot = duco_ns.class_("DucoReboot", button.Button, cg.Parented.template(Duco))

TYPE_REBOOT = "reboot"

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_REBOOT: button.button_schema(DucoReboot).extend(DUCO_COMPONENT_SCHEMA),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await button.new_button(config)
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    await cg.register_parented(var, hub)

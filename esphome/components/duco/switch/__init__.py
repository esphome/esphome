import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, Duco, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoInstallerMode = duco_ns.class_("DucoInstallerMode", switch.Switch, cg.Parented.template(Duco))
DucoLocator = duco_ns.class_("DucoLocator", switch.Switch, cg.Parented.template(Duco))

TYPE_INSTALLER_MODE = "installer_mode"
TYPE_LOCATOR = "locator"

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_INSTALLER_MODE: switch.switch_schema(DucoInstallerMode).extend(DUCO_COMPONENT_SCHEMA),
        TYPE_LOCATOR: switch.switch_schema(DucoLocator)
        .extend(
            {
                cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=0x45),
            }
        )
        .extend(DUCO_COMPONENT_SCHEMA),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await switch.new_switch(config)
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    await cg.register_parented(var, hub)
    if config[CONF_TYPE] == TYPE_LOCATOR:
        cg.add(var.set_address(config[CONF_ADDRESS]))

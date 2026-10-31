import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, Duco, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoInstallerMode = duco_ns.class_("DucoInstallerMode", switch.Switch, cg.Parented.template(Duco))

TYPE_INSTALLER_MODE = "installer_mode"

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_INSTALLER_MODE: switch.switch_schema(DucoInstallerMode).extend(DUCO_COMPONENT_SCHEMA),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await switch.new_switch(config)
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    await cg.register_parented(var, hub)

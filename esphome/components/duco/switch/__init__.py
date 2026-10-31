import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS
from esphome.types import ConfigType

from .. import (
    CONF_ALLOW_DANGEROUS_CONTROLS,
    CONF_DUCO_ID,
    DUCO_COMPONENT_SCHEMA,
    Duco,
    duco_ns,
)

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoInstallerMode = duco_ns.class_("DucoInstallerMode", switch.Switch, cg.Parented.template(Duco))
DucoLocator = duco_ns.class_("DucoLocator", switch.Switch, cg.Parented.template(Duco))
DucoDangerousControls = duco_ns.class_(
    "DucoDangerousControls", switch.Switch, cg.Component, cg.Parented.template(Duco)
)

CONF_INSTALLER_MODE = "installer_mode"
CONF_LOCATOR = "locator"

DANGEROUS_CONTROLS_SCHEMA = switch.switch_schema(DucoDangerousControls).extend(cv.COMPONENT_SCHEMA)

INSTALLER_MODE_SCHEMA = switch.switch_schema(DucoInstallerMode)

LOCATOR_SCHEMA = switch.switch_schema(DucoLocator).extend(
    {
        cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=0x45),
    }
)

CONFIG_SCHEMA = DUCO_COMPONENT_SCHEMA.extend(
    {
        cv.Optional(CONF_ALLOW_DANGEROUS_CONTROLS): DANGEROUS_CONTROLS_SCHEMA,
        cv.Optional(CONF_INSTALLER_MODE): INSTALLER_MODE_SCHEMA,
        cv.Optional(CONF_LOCATOR): cv.ensure_list(LOCATOR_SCHEMA),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    if dangerous_controls_config := config.get(CONF_ALLOW_DANGEROUS_CONTROLS):
        var = await switch.new_switch(dangerous_controls_config)
        await cg.register_parented(var, hub)
        await cg.register_component(var, dangerous_controls_config)

    if installer_mode_config := config.get(CONF_INSTALLER_MODE):
        var = await switch.new_switch(installer_mode_config)
        await cg.register_parented(var, hub)

    for locator_config in config.get(CONF_LOCATOR, []):
        var = await switch.new_switch(locator_config)
        await cg.register_parented(var, hub)
        cg.add(var.set_address(locator_config[CONF_ADDRESS]))

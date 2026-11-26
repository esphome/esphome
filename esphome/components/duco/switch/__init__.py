import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA

DEPENDENCIES = ["duco"]

CONF_ALLOW_SYSTEM_CONFIGURATION = "allow_system_configuration"

CONFIG_SCHEMA = DUCO_COMPONENT_SCHEMA.extend(
    {
        cv.Required(CONF_ALLOW_SYSTEM_CONFIGURATION): switch.switch_schema(
            switch.ApplySwitch,
            block_inverted=True,
            default_restore_mode="ALWAYS_OFF",
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="switch",
    platform="duco",
    keys=(CONF_DUCO_ID,),
    message="Only one allow_system_configuration switch is allowed per Duco hub",
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_DUCO_CONFIGURATION_SWITCH")
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_ALLOW_SYSTEM_CONFIGURATION,
        hub.set_allow_system_configuration_switch,
        apply="set_allow_system_configuration(state)",
        publish="after",
    )

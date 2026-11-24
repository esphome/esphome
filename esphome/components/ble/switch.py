from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import switch
from esphome.components.api import wizard
from esphome.components.const import CONF_ENABLED
from esphome.components.esp32_ble import CONF_BLE_ID, ESP32BLE
from esphome.components.switch import DOMAIN as SWITCH_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

# Only the ESP32 BLE stack is supported for now
DEPENDENCIES = ["esp32_ble"]


def _register_wizard_role(config: ConfigType) -> ConfigType:
    """Give the switch the wizard role ble.<type>."""
    wizard.register_entity_role(config[CONF_ID], f"{DOMAIN}.{config[CONF_TYPE]}")
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            CONF_ENABLED: switch.switch_schema(
                switch.ApplySwitch,
                block_inverted=True,
                default_restore_mode="RESTORE_DEFAULT_ON",
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:bluetooth",
            ).extend({cv.GenerateID(CONF_BLE_ID): cv.use_id(ESP32BLE)}),
        },
        key=CONF_TYPE,
    ),
    _register_wizard_role,
)

# The hub keeps a single pointer to each type of switch.
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=SWITCH_DOMAIN,
    platform=DOMAIN,
    keys=(CONF_TYPE,),
    message="Only one ble '{type}' switch is allowed",
)


@dataclass(frozen=True, slots=True)
class _SwitchType:
    """How the code for one switch type is generated.

    Attributes:
        setter: Name of the hub method that is given the switch, e.g. ``set_enabled_switch``.
        apply: Hub call the switch makes when it is turned on or off, with ``state`` in scope,
            e.g. ``set_enabled(state)``.
    """

    setter: str
    apply: str


_SWITCH_TYPES: dict[str, _SwitchType] = {
    CONF_ENABLED: _SwitchType(setter="set_enabled_switch", apply="set_enabled(state)"),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BLE_ID])
    switches = switch.sub_switches(config, parent=hub)
    switch_type = _SWITCH_TYPES[config[CONF_TYPE]]
    # The hub restores the switch in setup() and publishes every enable or disable request.
    await switches.create(
        config, getattr(hub, switch_type.setter), apply=switch_type.apply
    )
    cg.add_define("USE_ESP32_BLE_SWITCH")

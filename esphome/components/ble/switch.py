import esphome.codegen as cg
from esphome.components import switch
from esphome.components.api import wizard
from esphome.components.const import CONF_ENABLED
from esphome.components.esp32_ble import CONF_BLE_ID, ESP32BLE
from esphome.components.switch import DOMAIN as SWITCH_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

# Only the ESP32 BLE stack is supported for now
DEPENDENCIES = ["esp32_ble"]


def _register_wizard_role(config: ConfigType) -> ConfigType:
    """Give the switch the wizard role ble.enabled."""
    wizard.register_entity_role(config[CONF_ID], f"{DOMAIN}.{CONF_ENABLED}")
    return config


CONFIG_SCHEMA = cv.All(
    switch.switch_schema(
        switch.ApplySwitch,
        block_inverted=True,
        default_restore_mode="RESTORE_DEFAULT_ON",
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:bluetooth",
    ).extend({cv.GenerateID(CONF_BLE_ID): cv.use_id(ESP32BLE)}),
    _register_wizard_role,
)

# The hub keeps a single pointer to the switch.
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=SWITCH_DOMAIN,
    platform=DOMAIN,
    message="Only one ble switch is allowed",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BLE_ID])
    switches = switch.sub_switches(config, parent=hub)
    # The hub restores the switch in setup() and publishes every enable or disable request.
    await switches.create(config, hub.set_enabled_switch, apply="set_enabled(state)")
    cg.add_define("USE_ESP32_BLE_SWITCH")

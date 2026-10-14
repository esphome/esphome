from functools import partial

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_ECOCOMFORT2_ID, ECOCOMFORT2_CLIENT_SCHEMA, ecocomfort2_ns

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

CONF_HUMIDITY_ADVANCED = "humidity_advanced"
CONF_VOC_ADVANCED = "voc_advanced"

Ecocomfort2Switch = ecocomfort2_ns.class_("Ecocomfort2Switch", switch.Switch)
SwitchSetting = ecocomfort2_ns.enum("SwitchSetting", is_class=True)

_SWITCHES = {
    CONF_HUMIDITY_ADVANCED: SwitchSetting.HUMIDITY_ADVANCED,
    CONF_VOC_ADVANCED: SwitchSetting.VOC_ADVANCED,
}

# The unit holds the setting, so there is nothing to restore
CONFIG_SCHEMA = cv.All(
    ECOCOMFORT2_CLIENT_SCHEMA.extend(
        {
            cv.Optional(key): switch.switch_schema(
                Ecocomfort2Switch,
                default_restore_mode="DISABLED",
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon="mdi:fan-plus",
            )
            for key in _SWITCHES
        }
    ),
    cv.has_at_least_one_key(*_SWITCHES),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ECOCOMFORT2_ID])
    switches = switch.sub_switches(config, parent=hub)
    for key, setting in _SWITCHES.items():
        await switches(key, partial(hub.set_switch, setting), setting)

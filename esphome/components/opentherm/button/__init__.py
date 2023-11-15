import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import DEVICE_CLASS_EMPTY
from ...opentherm import (
    OpenThermComponent,
    CONF_OPENTHERM_ID,
)
from .. import opentherm

CONF_BOILER_LO_RESET = "boiler_lo_reset"
CONF_CH_WATER_FILLING = "ch_water_filling"

ICON_RESET = "mdi:reset"
ICON_WATER_FILLING = "mdi:format-color-fill"


TYPES = [
    CONF_BOILER_LO_RESET,
    CONF_CH_WATER_FILLING,
]

DEPENDENCIES = ["opentherm"]

BoilerLOResetButton = opentherm.class_("BoilerLOResetButton", button.Button)
CHWaterFillingButton = opentherm.class_("CHWaterFillingButton", button.Button)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenThermComponent),
            cv.Optional(CONF_BOILER_LO_RESET): button.button_schema(
                BoilerLOResetButton,
                device_class=DEVICE_CLASS_EMPTY,
                icon=ICON_RESET,
            ),
            cv.Optional(CONF_CH_WATER_FILLING): button.button_schema(
                CHWaterFillingButton,
                device_class=DEVICE_CLASS_EMPTY,
                icon=ICON_WATER_FILLING,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if conf := config.get(key):
        var = await button.new_button(conf)
        await cg.register_parented(var, config[CONF_OPENTHERM_ID])
        cg.add(getattr(hub, f"set_{key}_button")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)

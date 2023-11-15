import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    ICON_RADIATOR,
)
from ...opentherm import (
    OpenThermComponent,
    CONF_OPENTHERM_ID,
)
from .. import opentherm

DEPENDENCIES = ["opentherm"]

OpenThermSwitch = opentherm.class_("OpenThermSwitch", switch.Switch, cg.Component)

CONF_CH_ENABLED = "ch_enabled"
CONF_CH_2_ENABLED = "ch_2_enabled"
CONF_DHW_ENABLED = "dhw_enabled"
CONF_COOLING_ENABLED = "cooling_enabled"
CONF_OTC_ACTIVE = "otc_active"

ICON_WATER_BOILER = "mdi:water-boiler"
ICON_SNOWFLAKE = "mdi:snowflake"
ICON_THERMOMETER_AUTO = "mdi:thermometer-auto"

TYPES = [
    CONF_CH_ENABLED,
    CONF_CH_2_ENABLED,
    CONF_DHW_ENABLED,
    CONF_COOLING_ENABLED,
    CONF_OTC_ACTIVE,
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenThermComponent),
            cv.Optional(CONF_CH_ENABLED): switch.switch_schema(
                OpenThermSwitch,
                icon=ICON_RADIATOR,
            ),
            cv.Optional(CONF_CH_2_ENABLED): switch.switch_schema(
                OpenThermSwitch,
                icon=ICON_RADIATOR,
            ),
            cv.Optional(CONF_DHW_ENABLED): switch.switch_schema(
                OpenThermSwitch,
                icon=ICON_WATER_BOILER,
            ),
            cv.Optional(CONF_COOLING_ENABLED): switch.switch_schema(
                OpenThermSwitch,
                icon=ICON_SNOWFLAKE,
            ),
            cv.Optional(CONF_OTC_ACTIVE): switch.switch_schema(
                OpenThermSwitch,
                icon=ICON_THERMOMETER_AUTO,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if conf := config.get(key):
        var = await switch.new_switch(conf)
        cg.add(getattr(hub, f"set_{key}_switch")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)

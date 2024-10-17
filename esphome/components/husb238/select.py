import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import (
    CONF_NAME,
    CONF_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
)
from . import husb238_ns, Husb238Component, CONF_HUSB238_ID

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["husb238"]

ICON_KNOB = "mdi:knob"

Husb238VoltageSelect = husb238_ns.class_("Husb238VoltageSelect", select.Select)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_HUSB238_ID): cv.use_id(Husb238Component),
    cv.Optional(CONF_VOLTAGE): cv.maybe_simple_value(
        select.select_schema(
            Husb238VoltageSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_KNOB,
        ),
        key=CONF_NAME,
    ),
}


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HUSB238_ID])

    if voltage_selector := config.get(CONF_VOLTAGE):
        s = await select.new_select(
            voltage_selector,
            options=["5V", "9V", "12V", "15V", "18V", "20V"],
        )
        await cg.register_parented(s, config[CONF_HUSB238_ID])
        cg.add(hub.set_voltage_select(s))

import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.core import coroutine

from ...mitsubishi_itp import CONF_MITSUBISHI_ITP_ID, MitsubishiUART, mitsubishi_itp_ns

CONF_FILTER_RESET_BUTTON = "filter_reset_button"

FilterResetButton = mitsubishi_itp_ns.class_(
    "FilterResetButton", button.Button, cg.Component
)

BUTTONS = {
    CONF_FILTER_RESET_BUTTON: button.button_schema(
        FilterResetButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:restore",
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MITSUBISHI_ITP_ID): cv.use_id(MitsubishiUART),
    }
).extend(
    {
        cv.Optional(button_designator): button_schema
        for button_designator, button_schema in BUTTONS.items()
    }
)


@coroutine
async def to_code(config):
    muart_component = await cg.get_variable(config[CONF_MITSUBISHI_ITP_ID])

    # Buttons
    for button_designator, _ in BUTTONS.items():
        button_conf = config[button_designator]
        button_component = await button.new_button(button_conf)
        await cg.register_component(button_component, button_conf)
        await cg.register_parented(button_component, muart_component)

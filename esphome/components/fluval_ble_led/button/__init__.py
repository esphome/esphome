import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, fluval_ble_led
from esphome.const import ICON_LIGHTBULB
from .. import fluval_ble_led_ns

CONF_MODE = "mode"

MODE_OPTIONS = {
    "MANUAL" : 0,
    "AUTO": 1,
    "PRO": 2
}

FluvalBleLedSelectModeButton = fluval_ble_led_ns.class_("FluvalBleLedSelectModeButton", button.Button, cg.Component, fluval_ble_led.FluvalBleLed)

CONFIG_SCHEMA = (
    button.button_schema(icon=ICON_LIGHTBULB)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(FluvalBleLedSelectModeButton),
            cv.Required(CONF_MODE): cv.enum(MODE_OPTIONS, upper=True)
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(fluval_ble_led.FLUVAL_CLIENT_SCHEMA)
)


async def to_code(config):
    var = await button.new_button(config)
    await button.register_button(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_trigger_mode(config[CONF_MODE]))
    await fluval_ble_led.register_fluval_led_client(var, config)

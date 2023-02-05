import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, fluval_ble_led
from esphome.const import ICON_LIGHTBULB
from .. import fluval_ble_led_ns

FluvalBleLedSwitch = fluval_ble_led_ns.class_(
    "FluvalBleLedSwitch", switch.Switch, cg.Component, fluval_ble_led.FluvalBleLed
)

CONFIG_SCHEMA = (
    switch.switch_schema(FluvalBleLedSwitch, icon=ICON_LIGHTBULB, block_inverted=True)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(fluval_ble_led.FLUVAL_CLIENT_SCHEMA)
)

async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await fluval_ble_led.register_fluval_led_client(var, config)

import esphome.codegen as cg
from esphome.components import text_sensor, uart
import esphome.config_validation as cv
from esphome.const import ICON_FINGERPRINT

CODEOWNERS = ["@hobbypunk90"]
DEPENDENCIES = ["uart"]
CONF_RESET = "reset"

wl134_ns = cg.esphome_ns.namespace("wl_134")
Wl134Component = wl134_ns.class_(
    "Wl134Component", text_sensor.TextSensor, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        Wl134Component,
        icon=ICON_FINGERPRINT,
    )
    .extend({cv.Optional(CONF_RESET, default=False): cv.boolean})
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_do_reset(config[CONF_RESET]))
    await uart.register_uart_device(var, config)

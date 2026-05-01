import esphome.codegen as cg
from esphome.components import light, uart
import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_OUTPUT_ID

CONF_USE_RM433_REMOTE = "use_rm433_remote"

DEPENDENCIES = ["uart", "light"]

sonoff_d1_ns = cg.esphome_ns.namespace("sonoff_d1")
SonoffD1Output = sonoff_d1_ns.class_(
    "SonoffD1Output", cg.Component, uart.UARTDevice, light.LightOutput
)

CONFIG_SCHEMA = (
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SonoffD1Output),
            cv.Optional(CONF_USE_RM433_REMOTE, default=False): cv.boolean,
            cv.Optional(CONF_MIN_VALUE, default=0): cv.int_range(min=0, max=100),
            cv.Optional(CONF_MAX_VALUE, default=100): cv.int_range(min=0, max=100),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "sonoff_d1", baud_rate=9600, require_tx=True, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_use_rm433_remote(config[CONF_USE_RM433_REMOTE]))
    cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    await light.register_light(var, config)

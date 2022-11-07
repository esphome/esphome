from esphome.components import climate
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

haier_ns = cg.esphome_ns.namespace("haier")
HaierClimate = haier_ns.class_(
    "HaierClimate", climate.Climate, cg.PollingComponent, uart.UARTDevice
)
SwingMode = haier_ns.enum("SwingMode")

CONF_SUPPORTED_SWING_MODE = "supported_swing_mode"
SWING_MODES = {
    "off": SwingMode.SWING_OFF,
    "vertical": SwingMode.SWING_VERTICAL,
    "horizontal": SwingMode.SWING_HORIZONTAL,
    "both": SwingMode.SWING_BOTH,
}

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HaierClimate),
            cv.Optional(CONF_SUPPORTED_SWING_MODE, default="off"): cv.enum(
                SWING_MODES, lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_SUPPORTED_SWING_MODE])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uart.register_uart_device(var, config)

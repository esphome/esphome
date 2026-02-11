import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THROTTLE

CODEOWNERS = ["@jasstrong"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_RD03D_ID = "rd03d_id"
CONF_TRACKING_MODE = "tracking_mode"

rd03d_ns = cg.esphome_ns.namespace("rd03d")
RD03DComponent = rd03d_ns.class_("RD03DComponent", cg.Component, uart.UARTDevice)
TrackingMode = rd03d_ns.enum("TrackingMode", is_class=True)

TRACKING_MODES = {
    "single": TrackingMode.SINGLE_TARGET,
    "multi": TrackingMode.MULTI_TARGET,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RD03DComponent),
            cv.Optional(CONF_TRACKING_MODE): cv.enum(TRACKING_MODES, lower=True),
            cv.Optional(CONF_THROTTLE): cv.positive_time_period_milliseconds,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "rd03d",
    require_tx=False,
    require_rx=True,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_TRACKING_MODE in config:
        cg.add(var.set_tracking_mode(config[CONF_TRACKING_MODE]))

    if CONF_THROTTLE in config:
        cg.add(var.set_throttle(config[CONF_THROTTLE]))

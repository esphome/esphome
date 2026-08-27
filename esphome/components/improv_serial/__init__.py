import esphome.codegen as cg
from esphome.components import improv_base, uart
from esphome.components.esp32 import VARIANT_ESP32S3, get_esp32_variant
from esphome.components.logger import USB_CDC
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_HARDWARE_UART,
    CONF_ID,
    CONF_LOGGER,
    CONF_UART_ID,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["improv_base"]
CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["logger", "wifi"]

improv_serial_ns = cg.esphome_ns.namespace("improv_serial")

ImprovSerialComponent = improv_serial_ns.class_("ImprovSerialComponent", cg.Component)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ImprovSerialComponent),
            # YAML only: rewiring Improv onto another UART is not a knob for a
            # visual editor and the device builder must not expose it
            cv.Optional(CONF_UART_ID, visibility=cv.Visibility.YAML_ONLY): cv.use_id(
                uart.UARTComponent
            ),
        }
    )
    .extend(improv_base.IMPROV_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


_UART_FINAL_VALIDATE = uart.final_validate_device_schema(
    "improv_serial", require_tx=True, require_rx=True
)


def validate_transport(config: ConfigType) -> None:
    if CONF_UART_ID in config:
        # A dedicated UART bus is used; the logger's serial settings are irrelevant,
        # but the bus itself must be bidirectional and not claimed by another device
        _UART_FINAL_VALIDATE(config)
        return
    # The host logger has no serial port for Improv to share
    if CORE.is_host:
        raise cv.Invalid("improv_serial on the host platform requires uart_id")
    logger_conf = fv.full_config.get()[CONF_LOGGER]
    if logger_conf[CONF_BAUD_RATE] == 0:
        raise cv.Invalid("improv_serial requires the logger baud_rate to be not 0")
    if CORE.is_esp32 and (
        logger_conf[CONF_HARDWARE_UART] == USB_CDC
        and get_esp32_variant() == VARIANT_ESP32S3
    ):
        raise cv.Invalid(
            "improv_serial does not support the selected logger hardware_uart"
        )


FINAL_VALIDATE_SCHEMA = validate_transport


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await improv_base.setup_improv_core(var, config, "improv_serial")
    cg.add_define("USE_IMPROV_SERIAL")
    if (uart_id := config.get(CONF_UART_ID)) is not None:
        cg.add(var.set_uart(await cg.get_variable(uart_id)))
        cg.add_define("USE_IMPROV_SERIAL_UART")

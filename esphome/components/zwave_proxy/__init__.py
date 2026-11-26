import esphome.codegen as cg
from esphome.components import uart
from esphome.components.usb_uart import is_usb_uart_channel
from esphome.components.wifi import DOMAIN as WIFI_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_POWER_SAVE_MODE, CONF_UART_ID, CONF_WIFI
from esphome.core import ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]
DOMAIN = "zwave_proxy"

zwave_proxy_ns = cg.esphome_ns.namespace("zwave_proxy")
ZWaveProxy = zwave_proxy_ns.class_("ZWaveProxy", cg.Component, uart.UARTDevice)

_add_uart = cg.slot_list("ZWAVE_PROXY_UART_COUNT", "add_uart", DOMAIN)


def _uart_ids(config: ConfigType) -> list[ID]:
    """Return the configured UARTs as a list; an omitted uart_id is a single id."""
    uart_ids = config[CONF_UART_ID]
    return uart_ids if isinstance(uart_ids, list) else [uart_ids]


def final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    if (wifi_conf := full_config.get(WIFI_DOMAIN)) and (
        wifi_conf.get(CONF_POWER_SAVE_MODE).lower() != "none"
    ):
        raise cv.Invalid(
            f"{CONF_WIFI} {CONF_POWER_SAVE_MODE} must be set to 'none' when using Z-Wave proxy"
        )
    # A UART that cannot be unplugged always reports connected, so the proxy would never leave it
    uart_ids = _uart_ids(config)
    if len(uart_ids) > 1 and not all(
        is_usb_uart_channel(uart_id, full_config) for uart_id in uart_ids
    ):
        raise cv.Invalid(
            f"When {CONF_UART_ID} lists several UARTs, each one must be a usb_uart channel",
            path=[CONF_UART_ID],
        )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ZWaveProxy),
        cv.GenerateID(CONF_UART_ID): cv.Any(
            cv.All(cv.ensure_list(cv.use_id(uart.UARTComponent)), cv.Length(min=1)),
            # An omitted uart_id is one unnamed id, not a list, so its errors name uart_id
            cv.use_id(uart.UARTComponent),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    uarts = [await cg.get_variable(uart_id) for uart_id in _uart_ids(config)]
    cg.add(var.set_uart_parent(uarts[0]))
    # A single UART needs no list; the proxy only picks one when there are several
    if len(uarts) > 1:
        for uart_var in uarts:
            cg.add(_add_uart(var, uart_var))
    cg.add_define("USE_ZWAVE_PROXY")

import esphome.codegen as cg
from esphome.components import tcp_uart, uart
from esphome.components.const import (
    CONF_DATA_BITS,
    CONF_PARITY,
    CONF_ROLE,
    CONF_STOP_BITS,
)
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_DEBUG, CONF_ID, CONF_UART_ID
from esphome.cpp_generator import MockObjClass
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["tcp_uart"]
DOMAIN = "rfc2217_uart"
MULTI_CONF = True

CONF_TCP_UART_ID = "tcp_uart_id"

rfc2217_uart_ns = cg.esphome_ns.namespace("rfc2217_uart")
Rfc2217Base = rfc2217_uart_ns.class_("Rfc2217Base", cg.Component)
Rfc2217Client = rfc2217_uart_ns.class_(
    "Rfc2217Client", uart.VirtualUARTComponent, Rfc2217Base
)
Rfc2217Server = rfc2217_uart_ns.class_("Rfc2217Server", Rfc2217Base, uart.UARTDevice)

CONFIG_SCHEMA = cv.typed_schema(
    {
        # The line settings go to the access server.
        "client": cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Rfc2217Client),
                cv.Required(CONF_TCP_UART_ID): cv.use_id(tcp_uart.TcpUart),
                cv.Required(CONF_BAUD_RATE): cv.int_range(min=1, max=0xFFFFFFFF),
                cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=5, max=8),
                cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                    uart.UART_PARITY_OPTIONS, upper=True
                ),
                cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
                cv.Optional(CONF_DEBUG): uart.maybe_empty_debug,
            }
        ).extend(cv.COMPONENT_SCHEMA),
        "server": cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Rfc2217Server),
                cv.Required(CONF_TCP_UART_ID): cv.use_id(tcp_uart.TcpUart),
                # The tcp_uart is a UART too, so there is never a single one to default to.
                cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            }
        ).extend(cv.COMPONENT_SCHEMA),
    },
    key=CONF_ROLE,
    default_type="client",
    lower=True,
)


def _final_validate(config: ConfigType) -> ConfigType:
    # Another reader of either UART would split the bytes with this one.
    full_config = fv.full_config.get()
    owned_keys = [CONF_TCP_UART_ID]
    server = config[CONF_ROLE] == "server"
    if server:
        # A server on a UART without a wire would answer line changes that never happen.
        declared = full_config.get_config_for_path(
            full_config.get_path_for_id(config[CONF_UART_ID])
        )
        if isinstance(getattr(declared, "type", None), MockObjClass) and (
            declared.type.inherits_from(uart.VirtualUARTComponent)
            or declared.type.inherits_from(tcp_uart.TcpUart)
        ):
            raise cv.Invalid(
                "uart_id must be a hardware UART, not a tcp_uart or another virtual UART.",
                [CONF_UART_ID],
            )
        owned_keys.append(CONF_UART_ID)
    for key in owned_keys:
        uart.claim_exclusive(config, DOMAIN, key)
    if server:
        uart.final_validate_device_schema(DOMAIN, require_tx=True, require_rx=True)(
            config
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_tcp_uart(await cg.get_variable(config[CONF_TCP_UART_ID])))
    if config[CONF_ROLE] == "client":
        cg.add_define("USE_RFC2217_UART_CLIENT")
        uart.require_virtual_uart()
        cg.add(var.set_baud_rate(config[CONF_BAUD_RATE]))
        cg.add(var.set_data_bits(config[CONF_DATA_BITS]))
        cg.add(var.set_stop_bits(config[CONF_STOP_BITS]))
        if (parity := config[CONF_PARITY]) != "NONE":
            cg.add(var.set_parity(parity))
        if debug := config.get(CONF_DEBUG):
            cg.add_global(uart.uart_ns.using)
            await uart.debug_to_code(debug, var)
        return
    await uart.register_uart_device(var, config)
    # A hardware UART on ESP32 changes its line without a driver reinstall.
    full_id, _ = await cg.get_variable_with_full_id(config[CONF_UART_ID])
    if isinstance(full_id.type, MockObjClass) and full_id.type.inherits_from(
        uart.IDFUARTComponent
    ):
        cg.add(var.set_idf_uart(True))


# The client needs the virtual UART base, which only a client compiles in.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"rfc2217_client.cpp": "USE_RFC2217_UART_CLIENT"}
)

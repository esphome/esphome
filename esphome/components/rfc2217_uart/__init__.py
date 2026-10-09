import esphome.codegen as cg
from esphome.components import tcp_uart, uart
from esphome.components.const import CONF_ROLE
from esphome.components.tcp_uart import DOMAIN as TCP_UART_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_DUMMY_RECEIVER, CONF_ID, CONF_UART_ID
from esphome.core import CORE
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
Rfc2217Server = rfc2217_uart_ns.class_("Rfc2217Server", Rfc2217Base, uart.UARTDevice)

CONFIG_SCHEMA = cv.typed_schema(
    {
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
    lower=True,
)


def _reject_dummy_receiver(uart_conf: ConfigType) -> ConfigType:
    debug = uart_conf.get(CONF_DEBUG)
    if isinstance(debug, dict) and debug.get(CONF_DUMMY_RECEIVER):
        raise cv.Invalid(
            "dummy_receiver reads this UART and drops the bytes rfc2217_uart should forward.",
            [CONF_DEBUG, CONF_DUMMY_RECEIVER],
        )
    return uart_conf


def _final_validate(config: ConfigType) -> ConfigType:
    # Another reader of either UART would split the bytes with this one.
    full_config = fv.full_config.get()
    data = full_config.data.setdefault(DOMAIN, {})
    uart_id = str(config[CONF_UART_ID])
    if uart_id in {str(conf[CONF_ID]) for conf in full_config.get(TCP_UART_DOMAIN, [])}:
        raise cv.Invalid(
            "uart_id must be the hardware UART, not a tcp_uart.", [CONF_UART_ID]
        )
    for key in (CONF_TCP_UART_ID, CONF_UART_ID):
        owned_id = str(config[key])
        used = data.setdefault(key, set())
        if owned_id in used:
            raise cv.Invalid(
                f"The UART '{owned_id}' is already used by another 'rfc2217_uart' entry.",
                [key],
            )
        used.add(owned_id)
        # Grouped CI builds share one bus between components, like uart's pin check.
        if CORE.testing_mode:
            continue
        for domain, domain_conf in full_config.items():
            if domain != DOMAIN and uart.subtree_references_uart(domain_conf, owned_id):
                raise cv.Invalid(
                    f"The UART '{owned_id}' is also used by '{domain}'. "
                    "rfc2217_uart requires exclusive use of that UART.",
                    [key],
                )
    fv.id_declaration_match_schema(_reject_dummy_receiver)(config[CONF_UART_ID])
    uart.final_validate_device_schema(DOMAIN, require_tx=True, require_rx=True)(config)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_tcp_uart(await cg.get_variable(config[CONF_TCP_UART_ID])))
    await uart.register_uart_device(var, config)
    # A hardware UART on ESP32 changes its line without a driver reinstall.
    full_id, serial = await cg.get_variable_with_full_id(config[CONF_UART_ID])
    if isinstance(full_id.type, MockObjClass) and full_id.type.inherits_from(
        uart.IDFUARTComponent
    ):
        cg.add(var.set_idf_uart(serial))

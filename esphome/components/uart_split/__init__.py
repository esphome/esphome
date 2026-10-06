import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIRECTION,
    CONF_ID,
    CONF_OUTPUTS,
    CONF_RX_ONLY,
    CONF_UART_ID,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

uart_split_ns = cg.esphome_ns.namespace("uart_split")
UartSplit = uart_split_ns.class_("UartSplit", cg.Component)
UartSplitOutput = uart_split_ns.class_("UartSplitOutput", uart.UARTComponent)

MAX_OUTPUTS = 8


def _default_direction(config: ConfigType) -> ConfigType:
    for output in config[CONF_OUTPUTS]:
        if CONF_DIRECTION not in output:
            output[CONF_DIRECTION] = "BOTH" if output[CONF_RX_ONLY] else "RX"
    return config


def _inherit_settings(config: ConfigType) -> ConfigType:
    # Devices on an output are checked against the baud rate and framing of the UART it copies.
    for output in config[CONF_OUTPUTS]:
        uart.inherit_settings(output[CONF_ID], config[CONF_UART_ID])
    return config


OUTPUT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UartSplitOutput),
        cv.Optional(CONF_RX_ONLY, default=False): cv.boolean,
        cv.Optional(CONF_DIRECTION): cv.one_of("RX", "BOTH", upper=True),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UartSplit),
            cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Required(CONF_OUTPUTS): cv.All(
                cv.ensure_list(OUTPUT_SCHEMA),
                cv.Length(min=1, max=MAX_OUTPUTS),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _default_direction,
    _inherit_settings,
)


def _subtree_has_uart(node: object, uart_id: str) -> bool:
    if isinstance(node, dict):
        return any(
            (key == CONF_UART_ID and str(value) == uart_id)
            or _subtree_has_uart(value, uart_id)
            for key, value in node.items()
        )
    if isinstance(node, list):
        return any(_subtree_has_uart(item, uart_id) for item in node)
    return False


def _final_validate(config: ConfigType) -> ConfigType:
    # This component is the only reader. A second consumer would take the bytes.
    full_config = fv.full_config.get()
    owned = full_config.data.setdefault("uart_split", set())
    uart_id = str(config[CONF_UART_ID])
    if uart_id in owned:
        raise cv.Invalid(
            f"The UART '{uart_id}' is already read by another uart_split.",
            [CONF_UART_ID],
        )
    owned.add(uart_id)
    for domain, domain_conf in full_config.items():
        if domain == "uart_split":
            continue
        if _subtree_has_uart(domain_conf, uart_id):
            raise cv.Invalid(
                f"The UART '{uart_id}' is also used by '{domain}'. "
                "uart_split has to be the only reader; use one of its outputs.",
                [CONF_UART_ID],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    uart.require_virtual_uart()
    parent = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    for output_config in config[CONF_OUTPUTS]:
        output = cg.new_Pvariable(output_config[CONF_ID], var)
        if output_config[CONF_RX_ONLY]:
            cg.add(output.set_rx_only(True))
        if output_config[CONF_DIRECTION] == "BOTH":
            cg.add(output.set_mirror_tx(True))
        cg.add(var.add_output(output))

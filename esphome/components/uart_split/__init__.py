import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEBUG,
    CONF_DIRECTION,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
    CONF_OUTPUTS,
    CONF_RX_ONLY,
    CONF_UART_ID,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Bascht74"]
DEPENDENCIES = ["uart"]
DOMAIN = "uart_split"
MULTI_CONF = True

uart_split_ns = cg.esphome_ns.namespace("uart_split")
UartSplit = uart_split_ns.class_("UartSplit", cg.Component)
UartSplitOutput = uart_split_ns.class_("UartSplitOutput", uart.VirtualUARTComponent)

MAX_OUTPUTS = 8


def _default_direction(config: ConfigType) -> ConfigType:
    for output in config[CONF_OUTPUTS]:
        if CONF_DIRECTION not in output:
            output[CONF_DIRECTION] = "BOTH" if output[CONF_RX_ONLY] else "RX"
    return config


def _one_writer(config: ConfigType) -> ConfigType:
    # Two writers would share the transmit line and both read every answer.
    writers = [
        (index, output[CONF_ID])
        for index, output in enumerate(config[CONF_OUTPUTS])
        if not output[CONF_RX_ONLY]
    ]
    if len(writers) > 1:
        names = ", ".join(
            f"'{output_id.id}'" if output_id.is_manual else f"output {index}"
            for index, output_id in writers
        )
        raise cv.Invalid(
            f"Only one output may write, but {names} do. "
            "Set rx_only: true on all of them but one.",
            [CONF_OUTPUTS, writers[1][0]],
        )
    return config


def _inherit_settings(config: ConfigType) -> ConfigType:
    # Devices on an output are checked against the baud rate and framing of the UART it copies.
    for output in config[CONF_OUTPUTS]:
        uart.inherit_settings(output[CONF_ID], config[CONF_UART_ID])
        if output[CONF_RX_ONLY]:
            uart.mark_receive_only(output[CONF_ID])
    return config


OUTPUT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(UartSplitOutput),
        cv.Optional(CONF_RX_ONLY, default=False): cv.boolean,
        cv.Optional(CONF_DIRECTION): cv.one_of("RX", "BOTH", upper=True),
        cv.Optional(CONF_DEBUG): uart.maybe_empty_debug,
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
    _one_writer,
    _default_direction,
    _inherit_settings,
)


def _reject_dummy_receiver(uart_conf: ConfigType) -> ConfigType:
    debug = uart_conf.get(CONF_DEBUG)
    if isinstance(debug, dict) and debug.get(CONF_DUMMY_RECEIVER):
        raise cv.Invalid(
            "dummy_receiver reads this UART and takes the bytes uart_split should copy.",
            [CONF_DEBUG, CONF_DUMMY_RECEIVER],
        )
    return uart_conf


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
        if uart.subtree_references_uart(domain_conf, uart_id):
            raise cv.Invalid(
                f"The UART '{uart_id}' is also used by '{domain}'. "
                "uart_split has to be the only reader; use one of its outputs.",
                [CONF_UART_ID],
            )
    fv.id_declaration_match_schema(_reject_dummy_receiver)(config[CONF_UART_ID])
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
        if debug := output_config.get(CONF_DEBUG):
            cg.add_global(uart.uart_ns.using)
            await uart.debug_to_code(debug, output)
        cg.add(var.add_output(output))

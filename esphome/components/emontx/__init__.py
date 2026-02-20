from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMMAND,
    CONF_ID,
    CONF_ON_DATA,
    CONF_RX_BUFFER_SIZE,
    CONF_TRIGGER_ID,
    CONF_UART_ID,
)
import esphome.final_validate as fv

AUTO_LOAD = ["json"]
CODEOWNERS = ["@FredM67", "@TrystanLea", "@glynhudson"]
DEPENDENCIES = ["uart"]

emontx_ns = cg.esphome_ns.namespace("emontx")
EmonTx = emontx_ns.class_("EmonTx", cg.PollingComponent, uart.UARTDevice)

# Add trigger class for on_json
EmonTxJsonTrigger = emontx_ns.class_(
    "EmonTxJsonTrigger", automation.Trigger.template(cg.JsonObject)
)

# Add trigger class for on_data (fires for every serial line - raw data)
EmonTxDataTrigger = emontx_ns.class_(
    "EmonTxDataTrigger", automation.Trigger.template(cg.std_string)
)

# Action to send command to emonTx
EmonTxSendCommandAction = emontx_ns.class_("EmonTxSendCommandAction", automation.Action)

CONF_EMONTX_ID = "emontx_id"
CONF_TAG_NAME = "tag_name"
CONF_ON_JSON = "on_json"
CONF_CONFIG_PANEL = "config_panel"

# Main configuration schema
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmonTx),
            # Enable config panel - automatically fires esphome.emontx_raw events
            cv.Optional(CONF_CONFIG_PANEL, default=False): cv.boolean,
            # Add on_json trigger for handling JSON data
            cv.Optional(CONF_ON_JSON): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EmonTxJsonTrigger),
                }
            ),
            # Add on_data trigger for handling all serial lines (plain text + JSON)
            cv.Optional(CONF_ON_DATA): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EmonTxDataTrigger),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def final_validate(config):
    # TX is required if config_panel is enabled (send_command action requires config_panel)
    require_tx = config.get(CONF_CONFIG_PANEL, False)

    # Ensure UART RX buffer size is large enough to handle data bursts from firmware
    # The firmware can send ~2KB of configuration data in bursts which would
    # overflow the default 256-byte buffer causing data loss and corruption
    MINIMUM_RX_BUFFER_SIZE = 2048

    full_config = fv.full_config.get()

    # Find the UART component config and ensure rx_buffer_size is adequate
    for uart_conf in full_config["uart"]:
        if uart_conf[CONF_ID] == config[CONF_UART_ID]:
            current_buffer_size = uart_conf.get(CONF_RX_BUFFER_SIZE, 256)
            if current_buffer_size < MINIMUM_RX_BUFFER_SIZE:
                uart_id_str = str(config[CONF_UART_ID])

                # If value is exactly 256 (UART default), assume it's the default and auto-upgrade
                # If it's any other value below 2048, assume user explicitly set it and raise error
                if current_buffer_size == 256:
                    # Using default value - auto-upgrade with info message
                    uart_conf[CONF_RX_BUFFER_SIZE] = MINIMUM_RX_BUFFER_SIZE
                    import logging

                    _LOGGER = logging.getLogger(__name__)
                    _LOGGER.info(
                        "emontx: Automatically setting UART rx_buffer_size to %d bytes (minimum required by emonTx firmware)",
                        MINIMUM_RX_BUFFER_SIZE,
                    )
                else:
                    # User explicitly configured a non-default value that's too small
                    raise cv.Invalid(
                        f"Component emontx requires UART '{uart_id_str}' to have "
                        f"rx_buffer_size of at least {MINIMUM_RX_BUFFER_SIZE} bytes "
                        f"(currently set to {current_buffer_size} bytes). "
                        f"The emonTx firmware sends ~2KB configuration bursts that would overflow smaller buffers. "
                        f"Please increase rx_buffer_size in your uart configuration.",
                        path=[CONF_UART_ID],
                    )
            break

    # Validate UART settings
    schema = uart.final_validate_device_schema(
        "emontx",
        baud_rate=115200,
        require_tx=require_tx,
        require_rx=True,
        data_bits=8,
        parity="NONE",
        stop_bits=1,
    )
    return schema(config)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Set config_panel option
    cg.add(var.set_config_panel(config[CONF_CONFIG_PANEL]))

    # Enable HomeAssistant services feature when config_panel is enabled
    # This defines USE_API_HOMEASSISTANT_SERVICES which enables the event firing code
    if config[CONF_CONFIG_PANEL]:
        cg.add_define("USE_API_HOMEASSISTANT_SERVICES")

    # Process on_json triggers
    if CONF_ON_JSON in config:
        for conf in config[CONF_ON_JSON]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(
                trigger,
                [
                    (cg.JsonObject, "json"),
                    (cg.std_string, "raw_json"),
                ],
                conf,
            )

    # Process on_data triggers
    if CONF_ON_DATA in config:
        for conf in config[CONF_ON_DATA]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(
                trigger,
                [
                    (cg.std_string, "data"),
                ],
                conf,
            )


# Action: emontx.send_command

EMONTX_SEND_COMMAND_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(EmonTx),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "emontx.send_command", EmonTxSendCommandAction, EMONTX_SEND_COMMAND_ACTION_SCHEMA
)
async def emontx_send_command_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(template_))
    return var

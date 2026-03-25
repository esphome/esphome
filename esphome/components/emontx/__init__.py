from dataclasses import dataclass, field

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
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["json"]
CODEOWNERS = ["@FredM67", "@TrystanLea", "@glynhudson"]
DEPENDENCIES = ["uart"]

emontx_ns = cg.esphome_ns.namespace("emontx")
EmonTx = emontx_ns.class_("EmonTx", cg.Component, uart.UARTDevice)

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

DOMAIN = "emontx"

MINIMUM_RX_BUFFER_SIZE = 2048


@dataclass
class EmonTxData:
    sensor_counts: dict[str, int] = field(default_factory=dict)


def _get_data() -> EmonTxData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = EmonTxData()
    return CORE.data[DOMAIN]


def register_sensor(hub_id: str) -> None:
    """Called by the sensor platform to register itself with its hub."""
    data = _get_data()
    data.sensor_counts[hub_id] = data.sensor_counts.get(hub_id, 0) + 1


# Main configuration schema
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EmonTx),
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
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()

    # Ensure UART RX buffer size is large enough to handle data bursts from firmware
    for uart_conf in full_config["uart"]:
        if uart_conf[CONF_ID] == config[CONF_UART_ID]:
            current_buffer_size = uart_conf[CONF_RX_BUFFER_SIZE]
            if current_buffer_size < MINIMUM_RX_BUFFER_SIZE:
                raise cv.Invalid(
                    f"Component emontx requires UART '{config[CONF_UART_ID]}' to have "
                    f"rx_buffer_size of at least {MINIMUM_RX_BUFFER_SIZE} bytes "
                    f"(currently set to {current_buffer_size} bytes). "
                    f"Please add 'rx_buffer_size: {MINIMUM_RX_BUFFER_SIZE}' to your uart configuration.",
                    path=[CONF_UART_ID],
                )
            break

    # Validate UART settings
    schema = uart.final_validate_device_schema(
        "emontx",
        baud_rate=115200,
        require_tx=False,
        require_rx=True,
        data_bits=8,
        parity="NONE",
        stop_bits=1,
    )
    return schema(config)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Initialize sensor storage with count from register_sensor calls
    sensor_count = _get_data().sensor_counts.get(str(config[CONF_ID]), 0)
    if sensor_count > 0:
        cg.add(var.init_sensors(sensor_count))

    # Process trigger automations
    for conf_key, args in (
        (CONF_ON_JSON, [(cg.JsonObject, "json"), (cg.std_string, "raw_json")]),
        (CONF_ON_DATA, [(cg.std_string, "data")]),
    ):
        if conf_key in config:
            for conf in config[conf_key]:
                trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
                await automation.build_automation(trigger, args, conf)


# Action: emontx.send_command

EMONTX_SEND_COMMAND_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(EmonTx),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "emontx.send_command",
    EmonTxSendCommandAction,
    EMONTX_SEND_COMMAND_ACTION_SCHEMA,
    synchronous=True,
)
async def emontx_send_command_action_to_code(
    config: ConfigType, action_id, template_arg, args
) -> None:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(template_))
    return var

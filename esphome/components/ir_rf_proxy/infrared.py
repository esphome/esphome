"""Infrared platform implementation using remote_base (remote_transmitter/receiver)."""

from typing import Any

import esphome.codegen as cg
from esphome.components import infrared, remote_receiver, remote_transmitter
import esphome.config_validation as cv
from esphome.const import CONF_CARRIER_DUTY_PERCENT, CONF_FREQUENCY
import esphome.final_validate as fv

from . import CONF_REMOTE_RECEIVER_ID, CONF_REMOTE_TRANSMITTER_ID, ir_rf_proxy_ns

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["infrared"]

IrRfProxy = ir_rf_proxy_ns.class_("IrRfProxy", infrared.Infrared)

CONFIG_SCHEMA = cv.All(
    infrared.infrared_schema(IrRfProxy).extend(
        {
            cv.Optional(CONF_FREQUENCY, default=0): cv.frequency,
            cv.Optional(CONF_REMOTE_RECEIVER_ID): cv.use_id(
                remote_receiver.RemoteReceiverComponent
            ),
            cv.Optional(CONF_REMOTE_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
        }
    ),
    cv.has_exactly_one_key(CONF_REMOTE_RECEIVER_ID, CONF_REMOTE_TRANSMITTER_ID),
)


def _final_validate(config: dict[str, Any]) -> None:
    """Validate that transmitters have a proper carrier duty cycle."""
    # Only validate if this is an infrared (not RF) configuration with a transmitter
    if config.get(CONF_FREQUENCY, 0) != 0 or CONF_REMOTE_TRANSMITTER_ID not in config:
        return

    # Get the transmitter configuration
    transmitter_id = config[CONF_REMOTE_TRANSMITTER_ID]
    full_config = fv.full_config.get()
    transmitter_path = full_config.get_path_for_id(transmitter_id)[:-1]
    transmitter_config = full_config.get_config_for_path(transmitter_path)

    # Check if carrier_duty_percent set to 0 or 100
    # Note: remote_transmitter schema requires this field and validates 1-100%,
    # but we double-check here for infrared to provide a helpful error message
    duty_percent = transmitter_config.get(CONF_CARRIER_DUTY_PERCENT)
    if duty_percent in {0, 100}:
        raise cv.Invalid(
            f"Transmitter '{transmitter_id}' must have '{CONF_CARRIER_DUTY_PERCENT}' configured with "
            "an intermediate value (typically 30-50%) for infrared transmission. If this is an RF "
            f"transmitter, configure this infrared with a '{CONF_FREQUENCY}' value greater than 0"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: dict[str, Any]) -> None:
    """Code generation for remote_base infrared platform."""
    # Create and register the infrared entity
    var = await infrared.new_infrared(config)

    # Set frequency / 1000; zero indicates infrared hardware
    cg.add(var.set_frequency(config[CONF_FREQUENCY] / 1000))

    # Link transmitter if specified
    if CONF_REMOTE_TRANSMITTER_ID in config:
        transmitter = await cg.get_variable(config[CONF_REMOTE_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter))

    # Link receiver if specified
    if CONF_REMOTE_RECEIVER_ID in config:
        receiver = await cg.get_variable(config[CONF_REMOTE_RECEIVER_ID])
        cg.add(var.set_receiver(receiver))

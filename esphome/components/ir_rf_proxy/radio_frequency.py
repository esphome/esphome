"""Radio Frequency platform implementation using remote_base (remote_transmitter/receiver)."""

from typing import Any

import esphome.codegen as cg
from esphome.components import radio_frequency, remote_receiver, remote_transmitter
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY

from . import CONF_REMOTE_RECEIVER_ID, CONF_REMOTE_TRANSMITTER_ID, ir_rf_proxy_ns

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["radio_frequency"]

RfProxy = ir_rf_proxy_ns.class_("RfProxy", radio_frequency.RadioFrequency)

CONFIG_SCHEMA = cv.All(
    radio_frequency.radio_frequency_schema(RfProxy).extend(
        {
            cv.Optional(CONF_FREQUENCY): cv.frequency,
            cv.Optional(CONF_REMOTE_RECEIVER_ID): cv.use_id(
                remote_receiver.RemoteReceiverComponent
            ),
            cv.Optional(CONF_REMOTE_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_REMOTE_RECEIVER_ID, CONF_REMOTE_TRANSMITTER_ID),
)


async def to_code(config: dict[str, Any]) -> None:
    """Code generation for remote_base radio frequency platform."""
    var = await radio_frequency.new_radio_frequency(config)

    if CONF_FREQUENCY in config:
        cg.add(var.set_frequency_hz(int(config[CONF_FREQUENCY])))

    if CONF_REMOTE_TRANSMITTER_ID in config:
        transmitter = await cg.get_variable(config[CONF_REMOTE_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter))

    if CONF_REMOTE_RECEIVER_ID in config:
        receiver = await cg.get_variable(config[CONF_REMOTE_RECEIVER_ID])
        cg.add(var.set_receiver(receiver))

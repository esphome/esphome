"""Radio Frequency platform implementation using remote_base (remote_transmitter/receiver)."""

import esphome.codegen as cg
from esphome.components import radio_frequency, remote_receiver, remote_transmitter
import esphome.config_validation as cv
from esphome.const import CONF_CARRIER_DUTY_PERCENT, CONF_FREQUENCY
import esphome.final_validate as fv
from esphome.types import ConfigType

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
    cv.has_exactly_one_key(CONF_REMOTE_RECEIVER_ID, CONF_REMOTE_TRANSMITTER_ID),
)


def _final_validate(config: ConfigType) -> None:
    """Validate that RF transmitters have carrier duty set to 100%."""
    if CONF_REMOTE_TRANSMITTER_ID not in config:
        return

    full_config = fv.full_config.get()
    transmitter_path = full_config.get_path_for_id(config[CONF_REMOTE_TRANSMITTER_ID])[
        :-1
    ]
    transmitter_config = full_config.get_config_for_path(transmitter_path)

    duty_percent = transmitter_config.get(CONF_CARRIER_DUTY_PERCENT)
    if duty_percent is not None and duty_percent != 100:
        raise cv.Invalid(
            f"Transmitter '{config[CONF_REMOTE_TRANSMITTER_ID]}' must have "
            f"'{CONF_CARRIER_DUTY_PERCENT}' set to 100% for RF transmission. "
            "Dedicated RF hardware handles modulation; applying a carrier duty cycle "
            "would corrupt the signal"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
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

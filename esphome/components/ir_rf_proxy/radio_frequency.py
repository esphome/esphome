"""Radio Frequency platform implementation using remote_base (remote_transmitter/receiver)."""

import esphome.codegen as cg
from esphome.components import (
    cc1101,
    radio_frequency,
    remote_receiver,
    remote_transmitter,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CARRIER_DUTY_PERCENT,
    CONF_FREQUENCY,
    CONF_NUMBER,
    CONF_PIN,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_REMOTE_RECEIVER_ID, CONF_REMOTE_TRANSMITTER_ID, ir_rf_proxy_ns

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["radio_frequency"]

CONF_CC1101_ID = "cc1101_id"
CONF_GDO0_PIN = "gdo0_pin"
CONF_PACKET_MODE = "packet_mode"

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
            cv.Optional(CONF_CC1101_ID): cv.use_id(cc1101.CC1101Component),
        }
    ),
    cv.has_exactly_one_key(CONF_REMOTE_RECEIVER_ID, CONF_REMOTE_TRANSMITTER_ID),
)


def _get_referenced_config(config: ConfigType, id_key: str) -> ConfigType:
    """Look up the referenced component's config from the full validated config."""
    full_config = fv.full_config.get()
    path = full_config.get_path_for_id(config[id_key])[:-1]
    return full_config.get_config_for_path(path)


def _final_validate(config: ConfigType) -> None:
    """Validate carrier duty cycle and (if used) CC1101 integration."""
    # remote_transmitter must use 100% duty for RF — RF hardware handles modulation
    if CONF_REMOTE_TRANSMITTER_ID in config:
        transmitter_config = _get_referenced_config(config, CONF_REMOTE_TRANSMITTER_ID)
        duty_percent = transmitter_config.get(CONF_CARRIER_DUTY_PERCENT)
        if duty_percent is not None and duty_percent != 100:
            raise cv.Invalid(
                f"Transmitter '{config[CONF_REMOTE_TRANSMITTER_ID]}' must have "
                f"'{CONF_CARRIER_DUTY_PERCENT}' set to 100% for RF transmission. "
                "Dedicated RF hardware handles modulation; applying a carrier duty cycle "
                "would corrupt the signal"
            )

    if CONF_CC1101_ID not in config:
        return

    # CC1101 must be in async transparent mode (packet_mode: false)
    cc1101_config = _get_referenced_config(config, CONF_CC1101_ID)
    if cc1101_config.get(CONF_PACKET_MODE, False):
        raise cv.Invalid(
            f"CC1101 '{config[CONF_CC1101_ID]}' must have '{CONF_PACKET_MODE}' set to "
            "'false' (the default, async transparent mode) when used with ir_rf_proxy. "
            "Packet mode buffers bytes internally and is incompatible with raw-timing "
            "transmission through remote_transmitter/remote_receiver"
        )

    # The remote_transmitter/receiver pin must match the CC1101's gdo0_pin
    cc1101_gdo0 = cc1101_config.get(CONF_GDO0_PIN)
    if cc1101_gdo0 is None:
        raise cv.Invalid(
            f"CC1101 '{config[CONF_CC1101_ID]}' must have '{CONF_GDO0_PIN}' configured "
            "when used with ir_rf_proxy — the GDO0 pin carries the serial data stream "
            "to/from the remote_transmitter or remote_receiver"
        )
    cc1101_gdo0_num = cc1101_gdo0.get(CONF_NUMBER)

    side_key = (
        CONF_REMOTE_TRANSMITTER_ID
        if CONF_REMOTE_TRANSMITTER_ID in config
        else CONF_REMOTE_RECEIVER_ID
    )
    side_config = _get_referenced_config(config, side_key)
    side_pin = side_config.get(CONF_PIN, {}).get(CONF_NUMBER)
    if (
        side_pin is not None
        and cc1101_gdo0_num is not None
        and side_pin != cc1101_gdo0_num
    ):
        raise cv.Invalid(
            f"CC1101 '{config[CONF_CC1101_ID]}' '{CONF_GDO0_PIN}' (GPIO{cc1101_gdo0_num}) "
            f"must match the pin of '{config[side_key]}' (GPIO{side_pin}) — the "
            "remote_transmitter/receiver drives or reads the same physical pin as the "
            "CC1101's serial data line"
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

    if CONF_CC1101_ID in config:
        cc1101_var = await cg.get_variable(config[CONF_CC1101_ID])
        cg.add(var.set_cc1101(cc1101_var))
        # Tell CC1101 to stay out of GDO0's pin direction / interrupt management —
        # remote_transmitter or remote_receiver owns the pin matrix routing, and
        # CC1101 calling pin_mode() would detach it from the RMT peripheral.
        cg.add(cc1101_var.set_gdo0_managed_externally(True))

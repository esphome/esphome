"""Reliability layer using HDLC style acknowledgement and go-back-N retransmission."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIMEOUT
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import RELIABILITY, PacketInterface, packet_interface_ns

CONF_ACK_DELAY = "ack_delay"
CONF_MAX_QUEUE_LENGTH = "max_queue_length"
CONF_MAX_RETRIES = "max_retries"

HdlcReliability = packet_interface_ns.class_("HdlcReliability", PacketInterface)


def _ack_delay_within_timeout(config: ConfigType) -> ConfigType:
    """The peer, configured the same, must hear the acknowledgement before its timeout."""
    if (
        config[CONF_ACK_DELAY].total_milliseconds
        >= config[CONF_TIMEOUT].total_milliseconds
    ):
        raise cv.Invalid(
            f"Must be shorter than '{CONF_TIMEOUT}'", path=[CONF_ACK_DELAY]
        )
    return config


HDLC_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HdlcReliability),
            # Retransmitting more often than the main loop runs would exhaust the retries at once
            cv.Optional(CONF_TIMEOUT, default="200ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=50)),
            ),
            cv.Optional(
                CONF_ACK_DELAY, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_QUEUE_LENGTH, default=4): cv.int_range(1, 7),
            cv.Optional(CONF_MAX_RETRIES, default=5): cv.int_range(1, 255),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _ack_delay_within_timeout,
)


@RELIABILITY.register("hdlc", schema=HDLC_SCHEMA, single_peer=lambda config: True)
async def build_hdlc(config: ConfigType, lower: MockObj) -> MockObj:
    cg.add_define("USE_PACKET_INTERFACE_HDLC")
    var = cg.new_Pvariable(config[CONF_ID], lower)
    cg.add(var.set_ack_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_ack_delay(config[CONF_ACK_DELAY]))
    cg.add(var.set_max_queue_length(config[CONF_MAX_QUEUE_LENGTH]))
    cg.add(var.set_max_retries(config[CONF_MAX_RETRIES]))
    await cg.register_component(var, config)
    return var

import voluptuous as vol
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from esphome.const import (
    CONF_SENSORS,
    CONF_SOURCE,
    CONF_COMMAND,
    CONF_PAYLOAD,
    CONF_POSITION,
    CONF_BYTES,
)

from .. import EbusComponent, CONF_EBUS_ID, ebus_ns

AUTO_LOAD = ["ebus"]

EbusSensor = ebus_ns.class_("EbusSensor", sensor.Sensor, cg.Component)

CONF_TELEGRAM = "telegram"
CONF_SEND_POLL = "send_poll"
CONF_ADDRESS = "address"
CONF_DECODE = "decode"
CONF_DIVIDER = "divider"


SYN = 0xAA
ESC = 0xA9


def validate_address(address):
    if address == SYN:
        raise vol.Invalid("SYN symbol (0xAA) is not a valid address")
    if address == ESC:
        raise vol.Invalid("ESC symbol (0xA9) is not a valid address")
    return cv.hex_uint8_t(address)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EbusSensor),
        cv.GenerateID(CONF_EBUS_ID): cv.use_id(EbusComponent),
        cv.Required(CONF_SENSORS): cv.ensure_list(
            sensor.sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(EbusSensor),
                    cv.Required(CONF_TELEGRAM): cv.Schema(
                        {
                            cv.Optional(CONF_SEND_POLL, default=False): cv.boolean,
                            cv.Optional(CONF_ADDRESS): validate_address,
                            cv.Required(CONF_COMMAND): cv.hex_uint16_t,
                            cv.Required(CONF_PAYLOAD): cv.Schema([cv.hex_uint8_t]),
                            cv.Optional(CONF_DECODE): cv.Schema(
                                {
                                    cv.Optional(CONF_POSITION, default=0): cv.int_range(
                                        0, 15
                                    ),
                                    cv.Required(CONF_BYTES): cv.int_range(1, 4),
                                    cv.Required(CONF_DIVIDER): cv.float_,
                                }
                            ),
                        }
                    ),
                }
            )
        ),
    }
)


async def to_code(config):
    ebus = await cg.get_variable(config[CONF_EBUS_ID])

    for i, conf in enumerate(config[CONF_SENSORS]):
        print(f"Sensor: {i}, {conf}")
        sens = await sensor.new_sensor(conf)
        if CONF_SOURCE in conf[CONF_TELEGRAM]:
            cg.add(sens.set_source(conf[CONF_TELEGRAM][CONF_SOURCE]))

        if CONF_ADDRESS in conf[CONF_TELEGRAM]:
            cg.add(sens.set_address(conf[CONF_TELEGRAM][CONF_ADDRESS]))
        cg.add(sens.set_command(conf[CONF_TELEGRAM][CONF_COMMAND]))
        cg.add(sens.set_payload(conf[CONF_TELEGRAM][CONF_PAYLOAD]))
        cg.add(
            sens.set_response_read_position(
                conf[CONF_TELEGRAM][CONF_DECODE][CONF_POSITION]
            )
        )
        cg.add(
            sens.set_response_read_bytes(conf[CONF_TELEGRAM][CONF_DECODE][CONF_BYTES])
        )
        cg.add(
            sens.set_response_read_divider(
                conf[CONF_TELEGRAM][CONF_DECODE][CONF_DIVIDER]
            )
        )

        cg.add(ebus.add_receiver(sens))
        cg.add(ebus.add_sender(sens))

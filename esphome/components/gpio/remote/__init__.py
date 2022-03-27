from remoteprotocols.validators import kebab_to_pascal
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, pins
from esphome.components import remote
from esphome.const import (
    CONF_CARRIER_DUTY_PERCENT,
    CONF_ID,
    CONF_PROTOCOL,
)
from .. import gpio_ns


GPIORemote = gpio_ns.class_("GPIORemote", remote.Remote, cg.Component)


SUPPORTED_PROTOCOLS = ["duration"]

CONF_TRANSMIT_PIN = "transmit_pin"


CONFIG_SCHEMA = remote.REMOTE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GPIORemote),
        cv.Required(CONF_TRANSMIT_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CARRIER_DUTY_PERCENT): cv.All(
            cv.percentage_int, cv.Range(min=1, max=100)
        ),
        cv.Optional(CONF_PROTOCOL): cv.All(
            [cv.one_of(*SUPPORTED_PROTOCOLS)], cv.Length(min=1)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

PROTOCOL_REGISTRY = {}


def get_protocol(name):

    # make sure the protocol is valid at the Remote level

    # if more than one remote instance support a protocol, share de codec object
    if name in PROTOCOL_REGISTRY:
        return PROTOCOL_REGISTRY[name]

    class_name = kebab_to_pascal(name)

    ProtocolClass = gpio_ns.class_(
        f"RemoteProtocolCodec{class_name}", remote.RemoteProtocolCodec
    )
    codec = cg.new_Pvariable(
        core.ID(
            f"gpio_remoteprotocolcodec_{name}", is_declaration=True, type=ProtocolClass
        )
    )

    PROTOCOL_REGISTRY[name] = codec

    return codec


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_TRANSMIT_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)
    await cg.register_component(var, config)
    await remote.register_remote(var, config)

    cg.add(var.set_carrier_duty_percent(config[CONF_CARRIER_DUTY_PERCENT]))

    protocols = SUPPORTED_PROTOCOLS

    if CONF_PROTOCOL in config:
        protocols = config[CONF_PROTOCOL]

    for name in protocols:
        cg.add(var.add_protocol(get_protocol(name)))

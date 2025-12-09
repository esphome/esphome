import esphome.codegen as cg
from esphome.components import remote_receiver, remote_transmitter
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core.entity_helpers import setup_entity

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api"]
MULTI_CONF = True

infrared_proxy_ns = cg.esphome_ns.namespace("infrared_proxy")
InfraredProxyComponent = infrared_proxy_ns.class_(
    "InfraredProxyComponent", cg.Component, cg.EntityBase
)

CONF_REMOTE_TRANSMITTER_ID = "remote_transmitter_id"
CONF_REMOTE_RECEIVER_ID = "remote_receiver_id"


def _validate_transmitter_or_receiver(config):
    """Validate that at least one of transmitter or receiver is specified."""
    if (
        CONF_REMOTE_TRANSMITTER_ID not in config
        and CONF_REMOTE_RECEIVER_ID not in config
    ):
        raise cv.Invalid(
            "At least one of 'remote_transmitter_id' or 'remote_receiver_id' must be specified"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(InfraredProxyComponent),
            cv.Optional(CONF_REMOTE_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
            cv.Optional(CONF_REMOTE_RECEIVER_ID): cv.use_id(
                remote_receiver.RemoteReceiverComponent
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.ENTITY_BASE_SCHEMA),
    _validate_transmitter_or_receiver,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await setup_entity(var, config, "infrared_proxy")

    # Link transmitter if specified
    if CONF_REMOTE_TRANSMITTER_ID in config:
        transmitter = await cg.get_variable(config[CONF_REMOTE_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter))

    # Link receiver if specified
    if CONF_REMOTE_RECEIVER_ID in config:
        receiver = await cg.get_variable(config[CONF_REMOTE_RECEIVER_ID])
        cg.add(var.set_receiver(receiver))
        # Register the infrared_proxy component as a listener to the receiver
        cg.add(receiver.register_listener(var))

    # Add the global infrared_proxy define
    cg.add_define("USE_INFRARED_PROXY")

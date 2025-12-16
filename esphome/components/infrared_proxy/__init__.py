import esphome.codegen as cg
from esphome.components import remote_receiver, remote_transmitter
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core.entity_helpers import setup_entity

AUTO_LOAD = ["json"]
CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api"]
MULTI_CONF = True

infrared_proxy_ns = cg.esphome_ns.namespace("infrared_proxy")
InfraredProxyComponent = infrared_proxy_ns.class_(
    "InfraredProxyComponent", cg.Component, cg.EntityBase
)

CONF_HARDWARE_TYPE = "hardware_type"
CONF_REMOTE_RECEIVER_ID = "remote_receiver_id"
CONF_REMOTE_TRANSMITTER_ID = "remote_transmitter_id"

# Hardware type constants
HARDWARE_TYPE_INFRARED = "infrared"
HARDWARE_TYPE_RF_433 = "rf_433"
HARDWARE_TYPE_RF_900 = "rf_900"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(InfraredProxyComponent),
            cv.Required(CONF_HARDWARE_TYPE): cv.one_of(
                HARDWARE_TYPE_INFRARED, HARDWARE_TYPE_RF_433, HARDWARE_TYPE_RF_900
            ),
            cv.Optional(CONF_REMOTE_RECEIVER_ID): cv.use_id(
                remote_receiver.RemoteReceiverComponent
            ),
            cv.Optional(CONF_REMOTE_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.ENTITY_BASE_SCHEMA),
    cv.has_exactly_one_key(CONF_REMOTE_TRANSMITTER_ID, CONF_REMOTE_RECEIVER_ID),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await setup_entity(var, config, "infrared_proxy")

    # Set hardware type
    hardware_type = config[CONF_HARDWARE_TYPE]
    if hardware_type == HARDWARE_TYPE_INFRARED:
        cg.add(var.set_hardware_type(infrared_proxy_ns.HARDWARE_TYPE_INFRARED))
    elif hardware_type == HARDWARE_TYPE_RF_433:
        cg.add(var.set_hardware_type(infrared_proxy_ns.HARDWARE_TYPE_RF_433))
    elif hardware_type == HARDWARE_TYPE_RF_900:
        cg.add(var.set_hardware_type(infrared_proxy_ns.HARDWARE_TYPE_RF_900))

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

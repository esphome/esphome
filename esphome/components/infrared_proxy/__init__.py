import esphome.codegen as cg
from esphome.components import remote_receiver, remote_transmitter
import esphome.config_validation as cv
from esphome.const import CONF_CARRIER_DUTY_PERCENT, CONF_FREQUENCY, CONF_ID
from esphome.core.entity_helpers import setup_entity
import esphome.final_validate as fv

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api"]
MULTI_CONF = True

infrared_proxy_ns = cg.esphome_ns.namespace("infrared_proxy")
InfraredProxyComponent = infrared_proxy_ns.class_(
    "InfraredProxyComponent", cg.Component, cg.EntityBase
)

CONF_REMOTE_RECEIVER_ID = "remote_receiver_id"
CONF_REMOTE_TRANSMITTER_ID = "remote_transmitter_id"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(InfraredProxyComponent),
            cv.Optional(CONF_FREQUENCY, default=0): cv.frequency,
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


def _final_validate(config):
    """Validate that transmitters have a proper carrier duty cycle."""
    # Only validate if this is an infrared (not RF) configuration with a transmitter
    if config.get(CONF_FREQUENCY, 0) == 0 and CONF_REMOTE_TRANSMITTER_ID in config:
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
                f"transmitter, configure this infrared_proxy with a '{CONF_FREQUENCY}' value greater than "
                "0 and less than 100"
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await setup_entity(var, config, "infrared_proxy")

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
        # Register the infrared_proxy component as a listener to the receiver
        cg.add(receiver.register_listener(var))

    # Add the global infrared_proxy define
    cg.add_define("USE_INFRARED_PROXY")

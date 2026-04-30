import esphome.codegen as cg
from esphome.components import radio_frequency, remote_base
import esphome.config_validation as cv

remote_transmitter_ns = cg.esphome_ns.namespace("remote_transmitter")
TransmitterRadioFrequency = remote_transmitter_ns.class_(
    "TransmitterRadioFrequency", radio_frequency.RadioFrequency
)

CONF_TRANSMITTER_ID = remote_base.CONF_TRANSMITTER_ID

CONFIG_SCHEMA = radio_frequency.radio_frequency_schema(
    TransmitterRadioFrequency
).extend(
    {
        cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(
            remote_base.RemoteTransmitterBase
        ),
    }
)


async def to_code(config):
    var = await radio_frequency.new_radio_frequency(config)

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))

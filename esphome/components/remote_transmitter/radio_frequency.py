import esphome.codegen as cg
from esphome.components import radio_frequency, remote_base
import esphome.config_validation as cv
from esphome.const import CONF_CARRIER_DUTY_PERCENT
import esphome.final_validate as fv
from esphome.types import ConfigType

remote_transmitter_ns = cg.esphome_ns.namespace("remote_transmitter")
TransmitterRadioFrequency = remote_transmitter_ns.class_(
    "TransmitterRadioFrequency", radio_frequency.RadioFrequency
)

CONF_TRANSMITTER_ID = remote_base.CONF_TRANSMITTER_ID

CONFIG_SCHEMA = radio_frequency.radio_frequency_schema(
    TransmitterRadioFrequency
).extend(
    {
        cv.Required(CONF_TRANSMITTER_ID): cv.use_id(remote_base.RemoteTransmitterBase),
    }
)


def _final_validate(config: ConfigType) -> None:
    """Validate that RF transmitters have carrier duty set to 100%."""
    if CONF_TRANSMITTER_ID not in config:
        return

    transmitter_id = config[CONF_TRANSMITTER_ID]
    full_config = fv.full_config.get()
    transmitter_path = full_config.get_path_for_id(transmitter_id)[:-1]
    transmitter_config = full_config.get_config_for_path(transmitter_path)

    duty_percent = transmitter_config.get(CONF_CARRIER_DUTY_PERCENT)
    if duty_percent is not None and duty_percent != 100:
        raise cv.Invalid(
            f"Transmitter '{transmitter_id}' must have '{CONF_CARRIER_DUTY_PERCENT}' "
            "set to 100% for RF transmission. Dedicated RF hardware handles modulation; "
            "applying a carrier duty cycle would corrupt the signal"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = await radio_frequency.new_radio_frequency(config)

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))

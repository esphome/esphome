import logging

import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
from esphome.components.remote_receiver import CONF_RECEIVE_SYMBOLS
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, CONF_RMT_SYMBOLS, CONF_USE_DMA
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@duckle29"]


panasonic_ir_ns = cg.esphome_ns.namespace("panasonic_ir")
PanasonicIrClimate = panasonic_ir_ns.class_("PanasonicIrClimate", climate_ir.ClimateIR)
Model = panasonic_ir_ns.enum("Model", is_class=True)

MODELS = Model.options("DKE", "JKE", "LKE", "NKE", "CKP", "RKR", "PKE")

# A frame is 440 pulses, so the receiver needs room for 220 symbols (a mark and a space each)
FRAME_SYMBOLS = 220
RECOMMENDED_RMT_SYMBOLS = 256

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(PanasonicIrClimate).extend(
    {
        cv.Optional(CONF_MODEL, default="JKE"): cv.enum(MODELS, upper=True),
    }
)


def _final_validate(config: ConfigType) -> None:
    """Warn if the receiver is too small to capture a whole frame.

    The ESP32 receiver stops capturing when its RMT memory is full, and a frame cut short is silently ignored.
    This is a warning and not an error, as not every chip can allocate that much RMT memory.
    """
    receiver_id = config.get(remote_base.CONF_RECEIVER_ID)
    if receiver_id is None:
        return

    receiver_config = fv.get_declaring_config(receiver_id)

    # Only the RMT receivers on the ESP32 limit the capture: the receive buffer always, and the hardware memory
    # unless DMA streams the symbols out of it
    limits = [CONF_RECEIVE_SYMBOLS]
    if not receiver_config.get(CONF_USE_DMA):
        limits.append(CONF_RMT_SYMBOLS)
    if too_small := [
        f"{key}={symbols}"
        for key in limits
        if (symbols := receiver_config.get(key)) is not None and symbols < FRAME_SYMBOLS
    ]:
        _LOGGER.warning(
            "The receiver '%s' has %s, which is too small to capture a whole Panasonic frame (%d symbols). "
            "Frames from the remote will be cut short and ignored. "
            "Set %s and %s to %d on the receiver, if the chip allows it",
            receiver_id.id,
            " and ".join(too_small),
            FRAME_SYMBOLS,
            CONF_RMT_SYMBOLS,
            CONF_RECEIVE_SYMBOLS,
            RECOMMENDED_RMT_SYMBOLS,
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

import logging

import esphome.codegen as cg
from esphome.components import climate_ir, remote_base
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

    full_config = fv.full_config.get()
    receiver_path = full_config.get_path_for_id(receiver_id)[:-1]
    receiver_config = full_config.get_config_for_path(receiver_path)

    # Only the RMT receivers on the ESP32 have a hardware memory that limits the capture
    rmt_symbols = receiver_config.get(CONF_RMT_SYMBOLS)
    if rmt_symbols is None or receiver_config.get(CONF_USE_DMA):
        return
    if rmt_symbols < FRAME_SYMBOLS:
        _LOGGER.warning(
            "The receiver '%s' has %s=%d, which is too small to capture a whole Panasonic frame (%d symbols). "
            "Frames from the remote will be cut short and ignored. "
            "Set %s and receive_symbols to %d on the receiver, if the chip allows it",
            receiver_id.id,
            CONF_RMT_SYMBOLS,
            rmt_symbols,
            FRAME_SYMBOLS,
            CONF_RMT_SYMBOLS,
            RECOMMENDED_RMT_SYMBOLS,
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_model(config[CONF_MODEL]))

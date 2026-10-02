from dataclasses import dataclass
import logging

from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_PIN, CONF_ID, CONF_MODEL
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["i2c"]

CONF_ANALOG_GAIN = "analog_gain"
CONF_DAC_MODE = "dac_mode"
CONF_IGNORE_ENABLE_PIN_WARNING = "ignore_enable_pin_warning"
CONF_MIXER_MODE = "mixer_mode"
CONF_VOLUME_MIN_DB = "volume_min_db"
CONF_VOLUME_MAX_DB = "volume_max_db"
CONF_TAS58XX_ID = "tas58xx_id"

tas58xx_ns = cg.esphome_ns.namespace("tas58xx")
TAS58xx = tas58xx_ns.class_("TAS58xx", AudioDac, cg.PollingComponent, i2c.I2CDevice)

DacMode = tas58xx_ns.enum("DacMode")
DAC_MODES = {
    "btl": DacMode.DAC_MODE_BTL,
    "pbtl": DacMode.DAC_MODE_PBTL,
}

MixerMode = tas58xx_ns.enum("MixerMode")
MIXER_MODES = {
    "stereo": MixerMode.MIXER_MODE_STEREO,
    "stereo_inverse": MixerMode.MIXER_MODE_STEREO_INVERSE,
    "mono": MixerMode.MIXER_MODE_MONO,
    "left": MixerMode.MIXER_MODE_LEFT,
    "right": MixerMode.MIXER_MODE_RIGHT,
}


@dataclass(frozen=True)
class Model:
    """Limits of one model. The C++ side of a model is its ModelInfo constant."""

    model_info: MockObj
    analog_gain_min_db: float
    volume_min_db: float
    volume_max_db: float
    default_address: int


MODELS: dict[str, Model] = {
    "tas5805m": Model(
        model_info=tas58xx_ns.TAS5805M_MODEL,
        analog_gain_min_db=-15.5,
        volume_min_db=-103.0,
        volume_max_db=24.0,
        default_address=0x2D,
    ),
}


def _analog_gain_validator(model: Model):
    range_validator = cv.All(
        cv.decibel, cv.float_range(min=model.analog_gain_min_db, max=0.0)
    )

    def validator(value: float) -> float:
        value = range_validator(value)
        if value * 2 != int(value * 2):
            raise cv.Invalid("analog_gain must be a multiple of 0.5 dB")
        return value

    return validator


def _model_schema(model: Model) -> cv.Schema:
    volume_validator = cv.All(
        cv.decibel,
        cv.float_range(min=model.volume_min_db, max=model.volume_max_db),
    )
    return (
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(TAS58xx),
                cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
                cv.Optional(CONF_IGNORE_ENABLE_PIN_WARNING, default=False): cv.boolean,
                cv.Optional(
                    CONF_ANALOG_GAIN, default=model.analog_gain_min_db
                ): _analog_gain_validator(model),
                cv.Optional(CONF_DAC_MODE, default="btl"): cv.enum(
                    DAC_MODES, lower=True
                ),
                cv.Optional(CONF_MIXER_MODE, default="stereo"): cv.enum(
                    MIXER_MODES, lower=True
                ),
                cv.Optional(
                    CONF_VOLUME_MIN_DB, default=model.volume_min_db
                ): volume_validator,
                cv.Optional(
                    CONF_VOLUME_MAX_DB, default=model.volume_max_db
                ): volume_validator,
            }
        )
        .extend(cv.polling_component_schema("1s"))
        .extend(i2c.i2c_device_schema(model.default_address))
    )


def _validate_config(config: ConfigType) -> ConfigType:
    if config[CONF_VOLUME_MIN_DB] >= config[CONF_VOLUME_MAX_DB]:
        raise cv.Invalid(f"{CONF_VOLUME_MIN_DB} must be less than {CONF_VOLUME_MAX_DB}")
    if config[CONF_DAC_MODE] == "pbtl" and config[CONF_MIXER_MODE] in (
        "stereo",
        "stereo_inverse",
    ):
        raise cv.Invalid(
            f"{CONF_DAC_MODE} 'pbtl' drives a single speaker; use {CONF_MIXER_MODE} 'mono', 'left' or 'right'"
        )
    if CONF_ENABLE_PIN in config:
        if config[CONF_IGNORE_ENABLE_PIN_WARNING]:
            raise cv.Invalid(
                f"{CONF_IGNORE_ENABLE_PIN_WARNING} only applies when {CONF_ENABLE_PIN} is not set"
            )
    elif not config[CONF_IGNORE_ENABLE_PIN_WARNING]:
        # Without PDN high the device does not answer on I2C, and setup only reports an I2C failure
        _LOGGER.warning(
            "%s: %s not configured - if PDN (power down) is not hardwired high then add %s. "
            "Set %s: true to hide this warning",
            config[CONF_ID],
            CONF_ENABLE_PIN,
            CONF_ENABLE_PIN,
            CONF_IGNORE_ENABLE_PIN_WARNING,
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {name: _model_schema(model) for name, model in MODELS.items()},
        key=CONF_MODEL,
        lower=True,
    ),
    _validate_config,
)


TAS58XX_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(TAS58xx)})

for _name, _call in (
    ("tas58xx.activate", "activate()"),
    ("tas58xx.deactivate", "deactivate()"),
):
    automation.register_apply_action(
        _name, TAS58XX_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


async def to_code(config: ConfigType) -> None:
    model = MODELS[config[CONF_MODEL]]
    var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression(f"&{model.model_info}"))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_analog_gain(config[CONF_ANALOG_GAIN]))
    cg.add(var.set_dac_mode(config[CONF_DAC_MODE]))
    cg.add(var.set_mixer_mode(config[CONF_MIXER_MODE]))
    cg.add(var.set_volume_min_db(config[CONF_VOLUME_MIN_DB]))
    cg.add(var.set_volume_max_db(config[CONF_VOLUME_MAX_DB]))
    if enable_pin_config := config.get(CONF_ENABLE_PIN):
        enable_pin = await cg.gpio_pin_expression(enable_pin_config)
        cg.add(var.set_enable_pin(enable_pin))

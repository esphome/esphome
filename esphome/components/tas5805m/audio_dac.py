from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_PIN, CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

CONF_ANALOG_GAIN = "analog_gain"
CONF_DAC_MODE = "dac_mode"
CONF_MIXER_MODE = "mixer_mode"
CONF_VOLUME_MIN_DB = "volume_min_db"
CONF_VOLUME_MAX_DB = "volume_max_db"
CONF_TAS5805M_ID = "tas5805m_id"

tas5805m_ns = cg.esphome_ns.namespace("tas5805m")
TAS5805M = tas5805m_ns.class_("TAS5805M", AudioDac, cg.PollingComponent, i2c.I2CDevice)

DacMode = tas5805m_ns.enum("DacMode")
DAC_MODES = {
    "btl": DacMode.DAC_MODE_BTL,
    "pbtl": DacMode.DAC_MODE_PBTL,
}

MixerMode = tas5805m_ns.enum("MixerMode")
MIXER_MODES = {
    "stereo": MixerMode.MIXER_MODE_STEREO,
    "stereo_inverse": MixerMode.MIXER_MODE_STEREO_INVERSE,
    "mono": MixerMode.MIXER_MODE_MONO,
    "left": MixerMode.MIXER_MODE_LEFT,
    "right": MixerMode.MIXER_MODE_RIGHT,
}


def _validate_analog_gain(value: float) -> float:
    value = cv.All(cv.decibel, cv.float_range(min=-15.5, max=0.0))(value)
    if value * 2 != int(value * 2):
        raise cv.Invalid("analog_gain must be a multiple of 0.5 dB")
    return value


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
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TAS5805M),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_ANALOG_GAIN, default="-15.5dB"): _validate_analog_gain,
            cv.Optional(CONF_DAC_MODE, default="btl"): cv.enum(DAC_MODES, lower=True),
            cv.Optional(CONF_MIXER_MODE, default="stereo"): cv.enum(
                MIXER_MODES, lower=True
            ),
            cv.Optional(CONF_VOLUME_MIN_DB, default="-103dB"): cv.All(
                cv.decibel, cv.float_range(min=-103.0, max=24.0)
            ),
            cv.Optional(CONF_VOLUME_MAX_DB, default="24dB"): cv.All(
                cv.decibel, cv.float_range(min=-103.0, max=24.0)
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x2D)),
    _validate_config,
)


TAS5805M_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(TAS5805M)})

for _name, _call in (
    ("tas5805m.activate", "activate()"),
    ("tas5805m.deactivate", "deactivate()"),
):
    automation.register_apply_action(
        _name, TAS5805M_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
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

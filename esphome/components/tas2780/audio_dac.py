from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_POWER_MODE
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

tas2780_ns = cg.esphome_ns.namespace("tas2780")
TAS2780 = tas2780_ns.class_("TAS2780", AudioDac, cg.PollingComponent, i2c.I2CDevice)
ChannelSelect = tas2780_ns.enum("ChannelSelect")

CHANNELS = {
    "mono": ChannelSelect.MONO_DWN_MIX,
    "left": ChannelSelect.LEFT_CHANNEL,
    "right": ChannelSelect.RIGHT_CHANNEL,
}

CONF_VOL_RANGE_MIN = "vol_range_min"
CONF_VOL_RANGE_MAX = "vol_range_max"
CONF_AMP_LEVEL = "amp_level"

_AMP_LEVEL = cv.int_range(min=0, max=20)
_POWER_MODE = cv.int_range(min=0, max=3)


def _validate_vol_range(config: ConfigType) -> ConfigType:
    if config[CONF_VOL_RANGE_MIN] >= config[CONF_VOL_RANGE_MAX]:
        raise cv.Invalid(f"{CONF_VOL_RANGE_MIN} must be less than {CONF_VOL_RANGE_MAX}")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TAS2780),
            cv.Optional(CONF_AMP_LEVEL, default=8): _AMP_LEVEL,
            cv.Optional(CONF_POWER_MODE, default=2): _POWER_MODE,
            cv.Optional(CONF_VOL_RANGE_MIN, default=0.3): cv.percentage,
            cv.Optional(CONF_VOL_RANGE_MAX, default=1.0): cv.percentage,
            cv.Optional(CONF_CHANNEL, default="mono"): cv.enum(CHANNELS),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x38)),
    _validate_vol_range,
)


TAS2780_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(TAS2780)})

for _name, _call in (
    ("tas2780.deactivate", "deactivate()"),
    ("tas2780.reset", "reset()"),
):
    automation.register_apply_action(
        _name, TAS2780_ACTION_SCHEMA, automation.ApplyCall(_call)
    )

# Without power_mode the configured mode stays; activate() re-initializes only on a change.
automation.register_apply_action(
    "tas2780.activate",
    maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(TAS2780),
            cv.Optional(CONF_POWER_MODE): cv.templatable(_POWER_MODE),
        }
    ),
    automation.ApplyField(CONF_POWER_MODE, "set_power_mode", cg.uint8),
    automation.ApplyCall("activate()"),
)

automation.register_apply_action(
    "tas2780.update_config",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(TAS2780),
            cv.Optional(CONF_VOL_RANGE_MIN): cv.templatable(cv.percentage),
            cv.Optional(CONF_VOL_RANGE_MAX): cv.templatable(cv.percentage),
            cv.Optional(CONF_AMP_LEVEL): cv.templatable(_AMP_LEVEL),
            cv.Optional(CONF_CHANNEL): cv.templatable(cv.enum(CHANNELS)),
        }
    ).add_extra(
        cv.has_at_least_one_key(
            CONF_VOL_RANGE_MIN, CONF_VOL_RANGE_MAX, CONF_AMP_LEVEL, CONF_CHANNEL
        )
    ),
    automation.ApplyField(CONF_AMP_LEVEL, "set_amp_level", cg.uint8),
    automation.ApplyField(CONF_VOL_RANGE_MIN, "set_vol_range_min", cg.float_),
    automation.ApplyField(CONF_VOL_RANGE_MAX, "set_vol_range_max", cg.float_),
    automation.ApplyField(CONF_CHANNEL, "set_selected_channel", ChannelSelect),
    automation.ApplyCall("apply_config()"),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_amp_level(config[CONF_AMP_LEVEL]))
    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
    cg.add(var.set_vol_range_min(config[CONF_VOL_RANGE_MIN]))
    cg.add(var.set_vol_range_max(config[CONF_VOL_RANGE_MAX]))
    cg.add(var.set_selected_channel(config[CONF_CHANNEL]))

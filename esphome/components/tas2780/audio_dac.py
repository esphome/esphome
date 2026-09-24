from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_POWER_MODE
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
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


UpdateConfigAction = tas2780_ns.class_(
    "UpdateConfigAction", automation.Action, cg.Parented.template(TAS2780)
)

CONF_VOL_RANGE_MIN = "vol_range_min"
CONF_VOL_RANGE_MAX = "vol_range_max"
CONF_AMP_LEVEL = "amp_level"


def _validate_vol_range(config: ConfigType) -> ConfigType:
    if config[CONF_VOL_RANGE_MIN] >= config[CONF_VOL_RANGE_MAX]:
        raise cv.Invalid(f"{CONF_VOL_RANGE_MIN} must be less than {CONF_VOL_RANGE_MAX}")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TAS2780),
            cv.Optional(CONF_AMP_LEVEL, default=8): cv.int_range(min=0, max=20),
            cv.Optional(CONF_POWER_MODE, default=2): cv.int_range(min=0, max=3),
            cv.Optional(CONF_VOL_RANGE_MIN, default=0.3): cv.float_range(
                min=0.0, max=1.0
            ),
            cv.Optional(CONF_VOL_RANGE_MAX, default=1.0): cv.float_range(
                min=0.0, max=1.0
            ),
            cv.Optional(CONF_CHANNEL, default="mono"): cv.enum(CHANNELS),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x38)),
    _validate_vol_range,
)


TAS2780_BASE_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(TAS2780)})

# A missing power_mode activates with the configured one, see TAS2780::POWER_MODE_KEEP.
POWER_MODE_KEEP = 0xFF


def _default_power_mode_keep(config: ConfigType) -> ConfigType:
    config.setdefault(CONF_POWER_MODE, POWER_MODE_KEEP)
    return config


TAS2780_ACTIVATE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(TAS2780),
        cv.Optional(CONF_POWER_MODE): cv.templatable(cv.int_range(min=0, max=3)),
    }
).add_extra(_default_power_mode_keep)


automation.register_apply_action(
    "tas2780.deactivate",
    TAS2780_BASE_ACTION_SCHEMA,
    automation.ApplyCall("deactivate()"),
)

automation.register_apply_action(
    "tas2780.reset",
    TAS2780_BASE_ACTION_SCHEMA,
    automation.ApplyCall("reset()"),
)


automation.register_apply_action(
    "tas2780.activate",
    TAS2780_ACTIVATE_ACTION_SCHEMA,
    automation.ApplyField(CONF_POWER_MODE, "activate", cg.uint8),
)


TAS2780_UPDATE_CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(TAS2780),
        cv.Optional(CONF_VOL_RANGE_MIN): cv.templatable(
            cv.float_range(min=0.0, max=1.0)
        ),
        cv.Optional(CONF_VOL_RANGE_MAX): cv.templatable(
            cv.float_range(min=0.0, max=1.0)
        ),
        cv.Optional(CONF_AMP_LEVEL): cv.templatable(cv.int_range(min=0, max=20)),
        cv.Optional(CONF_CHANNEL): cv.templatable(cv.enum(CHANNELS)),
    }
)


@automation.register_action(
    "tas2780.update_config",
    UpdateConfigAction,
    TAS2780_UPDATE_CONFIG_SCHEMA,
    synchronous=True,
)
async def tas2780_update_config_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if (val := config.get(CONF_VOL_RANGE_MIN)) is not None:
        template = await cg.templatable(val, args, float)
        cg.add(var.set_vol_range_min(template))
    if (val := config.get(CONF_VOL_RANGE_MAX)) is not None:
        template = await cg.templatable(val, args, float)
        cg.add(var.set_vol_range_max(template))
    if (val := config.get(CONF_AMP_LEVEL)) is not None:
        template = await cg.templatable(val, args, cg.uint8)
        cg.add(var.set_amp_level(template))
    if (val := config.get(CONF_CHANNEL)) is not None:
        template = await cg.templatable(val, args, ChannelSelect)
        cg.add(var.set_channel(template))
    return var


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_amp_level(config[CONF_AMP_LEVEL]))
    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
    cg.add(var.set_vol_range_min(config[CONF_VOL_RANGE_MIN]))
    cg.add(var.set_vol_range_max(config[CONF_VOL_RANGE_MAX]))
    cg.add(var.set_selected_channel(config[CONF_CHANNEL]))
